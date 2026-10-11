#include "gs/gpu/gpu_gs_mirror.h"
#include <iostream>

void GpuGsMirror::attach(GsCommandSink& cpu) {
    gpu_.init();
    cpu.on_accepted=[this](std::span<const std::uint8_t> data){gpu_.transfer(data);};
    attached_=true;
}

void GpuGsMirror::compare(const GsCommandSink& cpu,unsigned frame) {
    const auto gpu=gpu_.readVram();
    std::size_t differing=0;
    for(std::size_t i=0;i<gpu.size();++i) {
        const auto w=cpu.word(i);
        if(!((w.value^gpu[i])&w.known))continue;
        if(!differing_frames_ && !differing) {first_frame_=frame;first_word_=i;first_cpu_=w.value;first_gpu_=gpu[i];}
        ++differing;
    }
    ++frames_;
    if(differing) {
        if(differing_frames_<8)std::cerr<<"GPU mirror: frame "<<frame<<": "<<differing<<" VRAM words differ\n";
        ++differing_frames_;
    }
}

DisplayFrame GpuGsMirror::scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field) {
    if(crt_mode!=2 && crt_mode!=3)throw std::runtime_error("GPU GS: unsupported SetGsCrt video mode");
    for(unsigned n=0;n<privileged.size();++n)gpu_.privileged(n,privileged[n]);
    gpu_.privileged(1,(32u<<3)|(crt_mode<<13)); // SMODE1: analog line clock (LC 32), CMOD NTSC/PAL.
    return gpu_.scanout(field);
}

GpuGsMirror::~GpuGsMirror() {
    if(!attached_ || !frames_)return;
    std::cerr<<"GPU mirror: "<<frames_<<" frames compared, "<<differing_frames_<<" differ";
    if(differing_frames_)std::cerr<<"; first at frame "<<first_frame_<<std::hex<<", word 0x"<<first_word_
        <<" (cpu 0x"<<first_cpu_<<", gpu 0x"<<first_gpu_<<")"<<std::dec;
    std::cerr<<std::endl;
}
