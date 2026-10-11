// paraLLEl-GS backend against the CPU GS model on synthetic GIF packets (no game data).
#include "gs/gpu_gs_backend.h"
#include "gs/gs_command_sink.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
// A+D-only PACKED packet: one GIF tag, then one register write per qword.
std::vector<std::uint8_t> packet(std::initializer_list<std::pair<std::uint64_t,std::uint64_t>> writes) {
    std::vector<std::uint8_t> bytes;
    auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);bytes.insert(bytes.end(),b,b+16);};
    q(writes.size()|(1ULL<<15)|(1ULL<<60),0xE); // NLOOP, EOP, PACKED, NREG 1: A+D.
    for(const auto& [reg,value]:writes)q(value,reg);
    return bytes;
}

// Every word the CPU model knows must equal the GPU's; returns the number compared.
std::size_t compare(const GsCommandSink& cpu,std::span<const std::uint32_t> gpu) {
    std::size_t compared=0;
    for(std::size_t i=0;i<gpu.size();++i) {
        const auto w=cpu.word(i);
        if(!w.known)continue;
        if((w.value&w.known)!=(gpu[i]&w.known)) {
            std::cerr<<"VRAM word "<<i<<" differs: cpu "<<std::hex<<w.value<<" gpu "<<gpu[i]<<'\n';
            throw std::runtime_error("GPU/CPU VRAM mismatch");
        }
        ++compared;
    }
    return compared;
}
}

int main() try {
    GpuGsBackend gpu;
    try {gpu.init();}
    catch(const std::exception& e) {std::cout<<"skipped: "<<e.what()<<'\n';return 77;}
    GsCommandSink cpu;
    // Flat sprite into a 64-pixel-wide PSMCT32 frame at page 0; depth writes masked.
    const auto sprite=packet({{0x4C,0x0000000000010000ULL},{0x4E,0x0000000101000020ULL},{0x18,0},
        {0x40,0x001F0000003F0000ULL},{0x47,0x30000},{0x00,6},{0x01,0x0000000080402010ULL},
        {0x05,0x0000000000400040ULL},{0x05,0x0000000001000100ULL}});
    cpu.transfer(sprite);gpu.transfer(sprite);
    const auto compared=compare(cpu,gpu.readVram());
    if(compared<12*12)throw std::runtime_error("Sprite covered fewer words than expected");
    // GPU-only mode: the CPU model validates and counts the sprite but draws nothing.
    GsCommandSink validator;validator.rasterize=false;validator.transfer(sprite);
    if(validator.frame_sprites!=1)throw std::runtime_error("Validate-only sink lost the sprite count");
    for(std::size_t i=0;i<1024*1024;++i)if(validator.word(i).known)throw std::runtime_error("Validate-only sink drew pixels");
    std::cout<<"GPU GS matches the CPU model on "<<compared<<" words\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
