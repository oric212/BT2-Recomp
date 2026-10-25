#include "gs/gpu/gpu_gs_mirror.h"
#include <iostream>

void GpuGsMirror::attach(GsCommandSink& cpu,unsigned super_sampling) {
    gpu_.init(super_sampling);
    startThread(cpu,0);
}

void GpuGsMirror::attachShared(GsCommandSink& cpu,GpuGsMirror& primary,unsigned super_sampling) {
    primary.drain();
    gpu_.initShared(primary.gpu_,super_sampling);
    startThread(cpu,1);
}

void GpuGsMirror::startThread(GsCommandSink& cpu,unsigned thread_index) {
    named_.on_accepted=[this](std::span<const std::uint8_t> data){gpu_.transfer(data);};
    // With texture names, transfers run again through the GS thread's sink, which hands them on in step.
    thread_=std::make_unique<GsRenderThread>([this](std::span<const std::uint8_t> data,bool transfer) {
        if(transfer && named_.texture_names)named_.transfer(data);else gpu_.transfer(data);
    },[thread_index]{GpuGsBackend::registerRenderThread(thread_index);});
    cpu.on_accepted=[this](std::span<const std::uint8_t> data){thread_->push(data,false);};
    cpu.on_accepted_transfer=[this](std::span<const std::uint8_t> data){thread_->push(data,true);};
}

void GpuGsMirror::nameTextures(GsTextureNames& names) {
    drain();
    named_.texture_names=&names;
}

void GpuGsMirror::enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names) {
    drain();
    gpu_.enableHdTextures(pack,budget_bytes,names);
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
    return gpu_.readback();
}

std::pair<unsigned,unsigned> GpuGsMirror::vsync(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    drain();
    if(crt_mode!=2 && crt_mode!=3)throw std::runtime_error("GPU GS: unsupported SetGsCrt video mode");
    for(unsigned n=0;n<privileged.size();++n)gpu_.privileged(n,privileged[n]);
    gpu_.privileged(1,(32u<<3)|(crt_mode<<13)); // SMODE1: analog line clock (LC 32), CMOD NTSC/PAL.
    return gpu_.vsync(field);
}

GpuGsMirror::~GpuGsMirror() {
    try {drain();} catch(const std::exception& e) {std::cerr<<"GPU GS: "<<e.what()<<std::endl;}
    thread_.reset();
    if(const auto hd=gpu_.hdTextureSummary();!hd.empty())std::cerr<<hd<<std::endl;
    if(const auto times=gpu_.timestampSummary();!times.empty())std::cerr<<times<<std::endl;
}
