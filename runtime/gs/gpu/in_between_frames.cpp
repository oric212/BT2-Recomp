#include "gs/gpu/in_between_frames.h"
#include "vu/vif1_blend.h"

InBetweenFrames::InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures) {
    sink_.resetDrawingState(); // The game reset the GS before the first frame; this sink starts there.
    sink_.rasterize=false;     // The renderer draws; the sink checks and names.
    mirror_.attachShared(sink_,primary,super_sampling);
    if(hd_textures) {sink_.texture_names=&names_;mirror_.shareHdTextures(primary,names_);}
    recording_.start=std::make_unique<Vif1>(vif1);
}

void InBetweenFrames::vif1Transfer(std::span<const std::uint32_t> words) {
    Event e;e.vif1=true;e.words.assign(words.begin(),words.end());
    recording_.events.push_back(std::move(e));
}

void InBetweenFrames::gsInput(std::span<const std::uint8_t> data,bool from_vif1) {
    auto& events=recording_.events;
    if(from_vif1 && !events.empty() && events.back().vif1) {events.back().output.emplace_back(data.begin(),data.end());return;}
    Event e;e.output.emplace_back(data.begin(),data.end());
    events.push_back(std::move(e));
}

void InBetweenFrames::send(std::span<const std::uint8_t> data) {
    if(!data.empty())sink_.transfer(data);
}

bool InBetweenFrames::drawInBetween(const Frame& older,const Frame& newer) {
    // The VIF1 transfers pair in order; each pair must pair code for code.
    std::vector<const Event*> a,b;
    for(const auto& e:older.events)if(e.vif1)a.push_back(&e);
    for(const auto& e:newer.events)if(e.vif1)b.push_back(&e);
    if(a.size()!=b.size() || b.empty() || !newer.start)return false;
    std::vector<std::vector<std::uint32_t>> blended;blended.reserve(b.size());
    for(std::size_t i=0;i<b.size();++i) {
        auto mid=blendVif1(a[i]->words,b[i]->words,0.5f);
        if(!mid)return false;
        blended.push_back(std::move(*mid));
    }
    // The newer frame's input with its lists blended, run on a copy of VIF1/VU1 as the frame started.
    Vif1 vif1=*newer.start;
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data){send(data);};
    std::size_t next=0;
    for(const auto& e:newer.events) {
        if(e.vif1)vif1.run(blended[next++],to_gs,to_gs);
        else for(const auto& data:e.output)send(data);
    }
    return true;
}

bool InBetweenFrames::flip(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    Frame finished=std::move(recording_);
    finished.fight=fight;
    recording_=Frame{};recording_.start=std::make_unique<Vif1>(vif1);
    bool ready=false;
    if(!replay_only && finished.fight && previous_.fight) {
        if(drawInBetween(previous_,finished)) {
            mirror_.flip();
            picture_=mirror_.vsync(privileged,crt_mode,field);
            ready=picture_.first!=0;
            if(ready)++drawn;
        } else ++unpaired;
    } else if(!replay_only) ++not_fights;
    // The real frame, so this renderer's state follows the main one's.
    for(const auto& e:finished.events)for(const auto& data:e.output)send(data);
    mirror_.flip();
    if(replay_only) {picture_=mirror_.vsync(privileged,crt_mode,field);ready=picture_.first!=0;}
    previous_=std::move(finished);
    previous_.start.reset(); // Only the newer frame's start is used.
    return ready;
}
