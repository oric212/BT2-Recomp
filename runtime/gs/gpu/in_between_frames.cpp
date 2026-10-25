#include "gs/gpu/in_between_frames.h"
#include "vu/vif1_blend.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>

InBetweenFrames::InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures)
    :primary_(primary),thread_([](std::span<const std::uint8_t>,bool){throw std::logic_error("60 fps battles: the thread takes tasks only");},
                               []{GpuGsBackend::registerRenderThread(1);}) {
    sink_.resetDrawingState(); // The game reset the GS before the first frame; this sink starts there.
    mirror_.attachShared(sink_,primary,super_sampling);
    names_.count_drawn=false; // Only the main renderer's names keep the naming log.
    if(hd_textures)mirror_.shareHdTextures(primary,names_); // The renderer's GS thread names its textures.
    recording_.start=std::make_unique<Vif1>(vif1);
}

InBetweenFrames::~InBetweenFrames() {
    try {thread_.drain();} catch(...) {} // A queued frame uses this.
    std::cerr<<std::dec<<"60 fps battles: "<<drawn<<" in-between pictures drawn, "<<shown<<" shown; "<<unpaired<<" fight frames unpaired, "
        <<not_fights<<" other frames, "<<camera_cuts<<" camera cuts, "<<blended_words<<" words blended ("<<realigned_pairs<<" shifted unpacks realigned), "<<refused_pairs<<" changed unpack pairs left as the newer frame's; "<<late<<" not ready in time (skipped); the game waited "<<waited_seconds*1000<<" ms for them; "
        <<(drawn?drawing_seconds*1000/drawn:0)<<" ms per in-between picture drawn (the longest "<<slowest_seconds*1000<<" ms)";
    if(names_.shared_hits+names_.shared_misses)std::cerr<<"; texture hashes taken from the main renderer's: "<<names_.shared_hits<<" of "<<names_.shared_hits+names_.shared_misses;
    std::cerr<<std::endl;
}

void InBetweenFrames::run() {
    Job job;
    {std::lock_guard<std::mutex> hold(lock_);job=std::move(job_);}
    bool ready=false;std::exception_ptr failure;
    const auto begin=std::chrono::steady_clock::now();
    try {ready=draw(std::move(job.frame),job);}
    catch(...) {failure=std::current_exception();}
    if(ready) {
        const double took=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
        drawing_seconds+=took;slowest_seconds=std::max(slowest_seconds,took);
    }
    std::lock_guard<std::mutex> hold(lock_);ready_=ready;failure_=failure;pending_=false;
}

bool InBetweenFrames::wait() {
    if(busy()) {
        const auto begin=std::chrono::steady_clock::now();
        thread_.drain();
        waited_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    }
    std::lock_guard<std::mutex> hold(lock_);
    if(failure_) {auto failure=failure_;failure_=nullptr;std::rethrow_exception(failure);}
    return ready_;
}

bool InBetweenFrames::busy() {
    std::lock_guard<std::mutex> hold(lock_);
    return pending_;
}

bool InBetweenFrames::keepsUp(bool ready) {
    recent_=(recent_<<1)|(ready?1u:0u);
    return ready && std::popcount(recent_)>=29;
}

bool InBetweenFrames::ready() {
    std::lock_guard<std::mutex> hold(lock_);
    return !pending_ && !failure_ && ready_;
}

void InBetweenFrames::start(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field,unsigned older_slot) {
    wait();
    // The recording becomes the job's frame; the oldest frame's buffers record the next one.
    Frame finished=std::move(recording_);
    finished.fight=fight;
    recording_=std::move(spare_);recording_.used=0;recording_.fight=false;recording_.start=std::make_unique<Vif1>(vif1);
    {
        std::lock_guard<std::mutex> hold(lock_);
        job_.frame=std::move(finished);job_.privileged=privileged;job_.crt_mode=crt_mode;job_.field=field;job_.older_slot=older_slot;
        ready_=false;pending_=true;
    }
    thread_.pushTask([this]{run();});
}

InBetweenFrames::Event& InBetweenFrames::Frame::next(bool vif1) {
    if(used==events.size())events.emplace_back();
    auto& e=events[used++];
    e.vif1=vif1;e.words.clear();e.output.clear();e.ends.clear();
    return e;
}

void InBetweenFrames::vif1Transfer(std::span<const std::uint32_t> words) {
    recording_.next(true).words.assign(words.begin(),words.end());
}

void InBetweenFrames::gsInput(std::span<const std::uint8_t> data,bool from_vif1) {
    auto& f=recording_;
    auto& e=from_vif1 && f.used && f.events[f.used-1].vif1?f.events[f.used-1]:f.next(false);
    e.output.insert(e.output.end(),data.begin(),data.end());e.ends.push_back(e.output.size());
}

void InBetweenFrames::send(std::span<const std::uint8_t> data) {
    if(!data.empty())sink_.transfer(data);
}

bool InBetweenFrames::drawInBetween(const Frame& older,const Frame& newer,unsigned older_slot) {
    // The VIF1 transfers pair in order; each pair must pair code for code.
    std::vector<const Event*> a,b;
    for(std::size_t n=0;n<older.used;++n)if(older.events[n].vif1)a.push_back(&older.events[n]);
    for(std::size_t n=0;n<newer.used;++n)if(newer.events[n].vif1)b.push_back(&newer.events[n]);
    if(a.size()!=b.size() || b.empty() || !newer.start)return false;
    // No in-between picture across a camera cut (vu/vif1_blend.h).
    for(std::size_t i=0;i<b.size();++i)if(cameraTurn(a[i]->words,b[i]->words)<0.906f) {++camera_cuts;return false;}
    std::vector<std::vector<std::uint32_t>> blended;blended.reserve(b.size());
    for(std::size_t i=0;i<b.size();++i) {
        std::size_t refused=0;
        std::size_t words=0,realigned=0;
        auto mid=blendVif1(a[i]->words,b[i]->words,scan_replay?1.0f:0.5f,&words,nullptr,1024,&refused,&realigned);
        refused_pairs+=refused;blended_words+=words;realigned_pairs+=realigned;
        if(!mid)return false;
        blended.push_back(std::move(*mid));
    }
    // The newer frame's input with its lists blended, run on a copy of VIF1/VU1 as the frame started and
    // drawn over the main renderer's GS state as it started.
    mirror_.takeState(primary_,older_slot);
    Vif1 vif1=*newer.start;
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data){send(data);};
    std::size_t next=0;
    for(std::size_t n=0;n<newer.used;++n) {
        const auto& e=newer.events[n];
        if(e.vif1)vif1.run(blended[next++],to_gs,to_gs);
        else e.eachOutput([this](std::span<const std::uint8_t> data){send(data);});
    }
    return true;
}

// On the thread: the in-between picture, or else the real frame.
bool InBetweenFrames::draw(Frame finished,const Job& job) {
    bool ready=false;
    // The second renderer only submits (its flushes leave the device's frame to the main renderer).
    if(!replay_only && finished.fight && previous_.fight && drawInBetween(previous_,finished,job.older_slot)) {
        mirror_.submit();
        picture_=mirror_.vsync(job.privileged,job.crt_mode,job.field);
        ready=picture_.first!=0;
        if(ready)++drawn;
    } else {
        if(!replay_only)++(finished.fight?unpaired:not_fights);
        for(std::size_t n=0;n<finished.used;++n)finished.events[n].eachOutput([this](std::span<const std::uint8_t> data){send(data);});
        mirror_.submit();
        if(replay_only) {picture_=mirror_.vsync(job.privileged,job.crt_mode,job.field);ready=picture_.first!=0;}
    }
    spare_=std::move(previous_);
    previous_=std::move(finished);
    previous_.start.reset(); // Only the newer frame's start is used.
    return ready;
}
