#include "gs/gpu/gpu_gs_mirror.h"
#include "host/thread_priority.h"
#include <iostream>

void GpuGsMirror::attach(GsCommandSink& cpu,unsigned super_sampling) {
    gpu_.init(super_sampling);
    startThread(cpu,0);
}

void GpuGsMirror::attachShared(GsCommandSink& cpu,GpuGsMirror& primary,unsigned super_sampling) {
    primary.drain();
    gpu_.initShared(primary.gpu_,super_sampling);
    startThread(cpu,1);
    primary_=&primary;primary.followers_.push_back(this);
}

void GpuGsMirror::whileFollowersPaused(const std::function<void()>& fn,std::size_t from) {
    if(from==followers_.size()) {fn();return;}
    auto& follower=*followers_[from];
    const auto rest=[&]{whileFollowersPaused(fn,from+1);};
    if(follower.thread_)follower.thread_->whilePaused([&follower]{std::lock_guard hold(follower.use_);follower.gpu_.submit();},rest);else rest();
}

void GpuGsMirror::whilePaused(const std::function<void()>& fn) {
    const auto followers=[&]{whileFollowersPaused(fn);};
    if(thread_)thread_->whilePaused([this]{std::lock_guard hold(use_);gpu_.submit();},followers);else followers();
}

WindowPresenter& GpuGsMirror::windowPresenter() {
    drain();
    // The presenter's thread waits for old GPU work first, then changes the device's frame while the GS threads stand still.
    return gpu_.windowPresenter([this](const std::function<void()>& frame){gpu_.awaitFrameChange();whilePaused(frame);});
}

void GpuGsMirror::startThread(GsCommandSink& cpu,unsigned thread_index) {
    named_.on_accepted=[this](std::span<const std::uint8_t> data){gpu_.transfer(data);};
    // With texture names, transfers run again through the GS thread's sink, which hands them on in step.
    thread_=std::make_unique<GsRenderThread>([this](std::span<const std::uint8_t> data,bool transfer) {
        if(transfer && named_.texture_names)named_.transfer(data);else gpu_.transfer(data);
    },[thread_index]{
        GpuGsBackend::registerRenderThread(thread_index);
        if(thread_index!=0)giveWayToTheGame(); // A second renderer's (the in-between pictures): optional work.
    });
    cpu.on_accepted=[this](std::span<const std::uint8_t> data){thread_->push(data,false);};
    cpu.on_accepted_transfer=[this](std::span<const std::uint8_t> data){thread_->push(data,true);};
}

void GpuGsMirror::nameTextures(GsTextureNames& names) {
    drain();
    named_.texture_names=&names;
}

void GpuGsMirror::enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names,bool playstation_buttons) {
    drain();
    gpu_.enableHdTextures(pack,budget_bytes,names,playstation_buttons);
    named_.texture_names=&names;
}

void GpuGsMirror::drain() {
    if(!thread_ || thread_->onThread())return;
    {
        FrameProfile::Scope timed(frame_profile.gs_wait);
        thread_->drain();
    }
    if(frame_profile.enabled)frame_profile.renderer+=thread_->takeBusySeconds();
}

DisplayFrame GpuGsMirror::scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    vsync(privileged,crt_mode,field);
    return readback();
}

std::pair<unsigned,unsigned> GpuGsMirror::vsync(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    const auto hold=use();
    if(crt_mode!=2 && crt_mode!=3)throw std::runtime_error("GPU GS: unsupported SetGsCrt video mode");
    for(unsigned n=0;n<privileged.size();++n)gpu_.privileged(n,privileged[n]);
    gpu_.privileged(1,(32u<<3)|(crt_mode<<13)); // SMODE1: analog line clock (LC 32), CMOD NTSC/PAL.
    return gpu_.vsync(field);
}

GpuGsMirror::~GpuGsMirror() {
    try {drain();} catch(const std::exception& e) {std::cerr<<"GPU GS: "<<e.what()<<std::endl;}
    if(primary_)std::erase(primary_->followers_,this);
    gpu_.closeWindowPresenter(); // Its thread pauses the GS thread.
    thread_.reset();
    if(const auto hd=gpu_.hdTextureSummary();!hd.empty())std::cerr<<hd<<std::endl;
    if(const auto times=gpu_.timestampSummary();!times.empty())std::cerr<<times<<std::endl;
}
