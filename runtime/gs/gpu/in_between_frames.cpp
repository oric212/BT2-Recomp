#include "gs/gpu/in_between_frames.h"
#include "host/thread_priority.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

InBetweenFrames::InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures,bool replay_only)
    :replay_only(replay_only),primary_(primary),
     thread_([](std::span<const std::uint8_t>,bool){throw std::logic_error("60 fps battles: the thread takes tasks only");},
             []{GpuGsBackend::registerRenderThread(1);giveWayToTheGame();}) {
    mirror_.attachShared(sink_,primary,super_sampling);
    names_.count_drawn=false; // Only the main renderer's names keep the naming log.
    // The renderer's GS thread names its textures; replays draw each frame after its flip.
    if(hd_textures)mirror_.shareHdTextures(primary,names_,replay_only);
    recording_.start=std::make_unique<Vif1>(vif1);
}

InBetweenFrames::~InBetweenFrames() {
    try {thread_.drain();} catch(...) {} // A queued frame uses this.
    std::cerr<<std::dec<<"60 fps battles: "<<drawn<<" in-between pictures drawn, "<<shown<<" shown; "<<unpaired<<" fight frames unpaired, "
        <<not_fights<<" other frames, "<<camera_cuts<<" camera cuts, "<<scene_changes<<" scene changes, "<<blended_words<<" words blended, "<<refused_pairs<<" changed unpack pairs left as the newer frame's; "<<late<<" not ready in time (skipped); the game waited "<<waited_seconds*1000<<" ms for them (over 2 ms at "<<paced_waits<<" flips); "
        <<(drawn?drawing_seconds*1000/drawn:0)<<" ms per in-between picture drawn (the longest "<<slowest_seconds*1000<<" ms)";
    if(names_.shared_hits+names_.shared_misses)std::cerr<<"; texture hashes taken from the main renderer's: "<<names_.shared_hits<<" of "<<names_.shared_hits+names_.shared_misses;
    std::cerr<<std::endl;
}

void InBetweenFrames::run() {
    Job job;
    {std::lock_guard<std::mutex> hold(lock_);job=std::move(jobs_.front());jobs_.pop_front();}
    bool ready=false;std::exception_ptr failure;
    try {ready=draw(std::move(job.frame),job);}
    catch(...) {failure=std::current_exception();}
    {
        std::lock_guard<std::mutex> hold(lock_);ready_=ready;if(failure && !failure_)failure_=failure;++finished_;
        held_[job.id%held_.size()]=Held{job.id,ready && !failure,picture_};
    }
    done_.notify_all();
}

bool InBetweenFrames::readyFor(std::uint64_t job) {
    std::lock_guard<std::mutex> hold(lock_);
    const auto& h=held_[job%held_.size()];return h.job==job && h.ready && !failure_;
}

std::pair<unsigned,unsigned> InBetweenFrames::pictureSizeFor(std::uint64_t job) {
    std::lock_guard<std::mutex> hold(lock_);
    const auto& h=held_[job%held_.size()];return h.job==job?h.size:std::pair<unsigned,unsigned>{0,0};
}

bool InBetweenFrames::wait() {
    // Also what is drawn as it comes, when no frame is pending.
    const auto begin=std::chrono::steady_clock::now();
    thread_.drain();
    waited_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::lock_guard<std::mutex> hold(lock_);
    if(failure_) {auto failure=failure_;failure_=nullptr;std::rethrow_exception(failure);}
    return ready_;
}

void InBetweenFrames::keepPace() {
    const auto begin=std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> hold(lock_);
    done_.wait(hold,[this]{return finished_+1>=started_ || failure_;});
    if(failure_) {auto failure=failure_;failure_=nullptr;std::rethrow_exception(failure);}
    const double waited=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    waited_seconds+=waited;
    const bool long_wait=waited>0.002;
    paced_waits+=long_wait;
    recent_waits_=(recent_waits_<<1)|(long_wait?1u:0u);
    if(std::popcount(recent_waits_)>16)
        throw std::runtime_error("the processor does not keep up with the in-between pictures (the game waited at "
                                 +std::to_string(std::popcount(recent_waits_))+" of the last 64 flips); turned off to keep the game's pace");
}

bool InBetweenFrames::busy() {
    std::lock_guard<std::mutex> hold(lock_);
    return finished_<started_;
}

bool InBetweenFrames::keepsUp(bool ready) {
    recent_=(recent_<<1)|(ready?1u:0u);
    return ready && std::popcount(recent_)>=29;
}

bool InBetweenFrames::ready() {
    std::lock_guard<std::mutex> hold(lock_);
    return finished_==started_ && !failure_ && ready_;
}

void InBetweenFrames::start(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field,unsigned next_slot) {
    {
        // Done at the flip already (wait or keepPace); kept for callers that start without it.
        std::unique_lock<std::mutex> hold(lock_);
        done_.wait(hold,[this]{return finished_+1>=started_ || failure_;});
    }
    // The recording becomes the job's frame; a frame the thread is done with records the next one.
    Frame finished=std::move(recording_);
    finished.fight=fight;
    {
        std::lock_guard<std::mutex> hold(lock_);
        if(free_.empty())recording_=Frame{};
        else {recording_=std::move(free_.back());free_.pop_back();}
    }
    recording_.used=0;recording_.handed=0;recording_.fight=false;recording_.streamed=false;
    recording_.start=std::make_unique<Vif1>(vif1);recording_.slot=next_slot;
    previous_fight_=fight && !replay_only; // The next frame is drawn as it comes.
    {
        std::lock_guard<std::mutex> hold(lock_);
        jobs_.push_back(Job{std::move(finished),privileged,crt_mode,field,started_});
        ready_=false;++started_;
    }
    thread_.pushTask([this]{run();});
}

InBetweenFrames::Event& InBetweenFrames::Frame::next(bool vif1) {
    if(used==events.size())events.emplace_back();
    auto& e=events[used++];
    e.vif1=vif1;e.words.clear();e.output.clear();e.ends.clear();e.walked=false;
    return e;
}

void InBetweenFrames::vif1Transfer(std::span<const std::uint32_t> words) {
    recording_.next(true).words.assign(words.begin(),words.end());
    if(previous_fight_)hand();
}

void InBetweenFrames::gsInput(std::span<const std::uint8_t> data,bool from_vif1) {
    auto& f=recording_;
    auto& e=from_vif1 && f.used && f.events[f.used-1].vif1?f.events[f.used-1]:f.next(false);
    e.output.insert(e.output.end(),data.begin(),data.end());e.ends.push_back(e.output.size());
}

void InBetweenFrames::hand() {
    auto& f=recording_;
    std::vector<const Event*> events;
    for(std::size_t n=f.handed;n<f.used;++n)events.push_back(&f.events[n]);
    if(events.empty())return;
    const bool first=!f.streamed;
    f.streamed=true;f.handed=f.used;
    // Its failure is the frame's (wait rethrows it); nothing is drawn after one.
    thread_.pushTask([this,events=std::move(events),first,start=f.start.get(),slot=f.slot] {
        {std::lock_guard<std::mutex> hold(lock_);if(failure_)return;}
        try {drawEvents(events,first,start,slot);}
        catch(...) {std::lock_guard<std::mutex> hold(lock_);if(!failure_)failure_=std::current_exception();}
    });
}

void InBetweenFrames::send(std::span<const std::uint8_t> data) {
    if(!data.empty())sink_.transfer(data);
}

// On the thread: the newer frame's input with its lists blended against the older frame's, run on a copy of
// VIF1/VU1 as the frame started and drawn over the main renderer's GS state as it started. The VIF1 transfers
// pair in order and each pair must pair code for code; no in-between picture across a camera cut
// (vu/vif1_blend.h). Once a pair fails the rest runs as it is (that picture is not shown).
void InBetweenFrames::drawEvents(const std::vector<const Event*>& events,bool first,const Vif1* start,unsigned slot) {
    const auto begin=std::chrono::steady_clock::now();
    if(first) {
        stream_=Stream{};
        for(std::size_t n=0;n<previous_.used;++n)if(previous_.events[n].vif1)stream_.older.push_back(&previous_.events[n]);
        stream_.vif1.emplace(*start);
        mirror_.takeState(primary_,slot);
    }
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data){send(data);};
    for(const auto* e:events) {
        if(!e->vif1) {e->eachOutput([this](std::span<const std::uint8_t> data){send(data);});continue;}
        const Event* older=stream_.next<stream_.older.size()?stream_.older[stream_.next]:nullptr;
        ++stream_.next;
        std::optional<std::vector<std::uint32_t>> mid;
        if(stream_.paired && older) {
            const auto* x=older->walk();const auto* y=e->walk();
            const auto pairing=x && y?pairVif1(*x,*y):std::nullopt;
            if(!pairing)stream_.paired=false;
            else if(cameraTurn(*pairing,older->words,e->words)<0.906f) {++camera_cuts;stream_.paired=false;}
            else {
                std::size_t refused=0,words=0;
                mid=blendVif1(*pairing,older->words,e->words,scan_replay?1.0f:0.5f,&words,nullptr,&refused);
                refused_pairs+=refused;blended_words+=words;
                if(!sameScene(refused,pairing->pairs.size())) {++scene_changes;stream_.paired=false;mid.reset();} // vu/vif1_blend.h
            }
        } else stream_.paired=false;
        stream_.vif1->run(mid?std::span<const std::uint32_t>(*mid):std::span<const std::uint32_t>(e->words),to_gs,to_gs);
    }
    mirror_.submit(); // The device's frame changes never wait for an open command buffer of this renderer.
    stream_.seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
}

// On the thread, at the newer frame's flip: the in-between picture drawn as the frame came, or else the
// real frame. The second renderer only submits (its flushes leave the device's frame to the main renderer).
bool InBetweenFrames::draw(Frame finished,const Job& job) {
    const auto begin=std::chrono::steady_clock::now();
    bool ready=false;
    if(finished.streamed) {
        for(std::size_t n=finished.handed;n<finished.used;++n)finished.events[n].eachOutput([this](std::span<const std::uint8_t> data){send(data);});
        mirror_.submit();
        if(finished.fight && stream_.paired && !stream_.older.empty() && stream_.next==stream_.older.size()) {
            picture_=mirror_.vsync(job.privileged,job.crt_mode,job.field);
            ready=picture_.first!=0;
            if(ready)mirror_.holdPicture(unsigned(job.id%GpuGsBackend::held_slots)); // Shown a field later in delayed fights.
        }
        if(ready) {
            ++drawn;
            const double took=stream_.seconds+std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            drawing_seconds+=took;slowest_seconds=std::max(slowest_seconds,took);
        } else ++(finished.fight?unpaired:not_fights);
    } else {
        if(!replay_only)++(finished.fight?unpaired:not_fights);
        for(std::size_t n=0;n<finished.used;++n)finished.events[n].eachOutput([this](std::span<const std::uint8_t> data){send(data);});
        mirror_.submit();
        if(replay_only) {picture_=mirror_.vsync(job.privileged,job.crt_mode,job.field);ready=picture_.first!=0;}
    }
    {std::lock_guard<std::mutex> hold(lock_);free_.push_back(std::move(previous_));}
    previous_=std::move(finished);
    previous_.start.reset(); // Only the newer frame's start is used.
    return ready;
}
