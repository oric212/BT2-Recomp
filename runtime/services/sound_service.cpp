#include "ee/ee_runtime.h"
#include <array>
#include <cstring>
#include <iostream>

// Native sound driver (SDRDRV, 0x80000701) and SOUNDS (0x02000004) services. Replies as the
// original modules returned them (traced 2026-10-11); audio output itself is a later step.
// - SDRDRV: 64-byte commands, 16-byte replies. 0xE620 initializes (zeros); 0x80D0 transfers a
//   sound block from IOP memory to sound RAM and returns its size (word 5); 0x80F0 returns 1.
// - SOUNDS: 256-byte commands, 64-byte replies. 0x10 initializes (zeros). From 0x20 on every reply is
//   the module's status block: words 4 and 12 are 1 and 2, word 5 counts the loaded sound banks, words
//   8 and 13 are module-local values (the worker's IOP thread ID and an IOP address; opaque).
//   PCSX2 call log 2026-10-12 (capture_ee.py --sounds-log, 40 calls from boot into the menus):
//   0x20 loads a bank (word 5 + 1) and starts the worker on the first; 0x30 unloads one (word 5 - 1);
//   0x40 (request a sound), 0x50 and 0x200 leave the block unchanged.
//   0x60, 0x80 and 0x100 are not in the log: they are answered with the unchanged block as well, which
//   is an inference from the module's uniform replies, not a captured result.

namespace {
constexpr std::uint32_t sdr_init=0xE620,sdr_block_transfer=0x80D0,sdr_command=0x80F0;
constexpr std::uint32_t sounds_init=0x10,sounds_bank=0x20,sounds_unload=0x30,sounds_play=0x40,sounds_50=0x50,sounds_60=0x60,sounds_80=0x80,sounds_100=0x100,sounds_200=0x200;
constexpr std::array<std::uint32_t,16> sounds_status={0,0,0,0,1,1,0,0,0x20010,0,0,0,2,0x1C2440,0,0};
}

bool EeRuntime::nativeSoundDriverCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=64 || r.recv_size!=16)throw UnsupportedSyscall(0x77,context->pc);
    std::array<std::uint32_t,4> reply{};
    if(on_sound_evidence) {checkMemory(rdram,r.send_source,64);on_sound_evidence("driver",r.rpc_number,std::span<const std::uint8_t>(memory_.data()+r.send_source,64));}
    switch(r.rpc_number) {
    case sdr_init: break;
    case sdr_block_transfer: {
        // {reply buffer, 1, 0, IOP source, sound RAM address, size, ...}: the source must be uploaded data.
        const auto source=read32(rdram,r.send_source+12),size=read32(rdram,r.send_source+20);
        if(!size || source+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
        const auto address=read32(rdram,r.send_source+16);
        try {sound_sampler.upload(address,std::span<const std::uint8_t>(native_iop_ram_.data()+source,size));}
        catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        if(on_sound_evidence)on_sound_evidence("samples",address,std::span<const std::uint8_t>(native_iop_ram_.data()+source,size));
        reply[0]=size;break;
    }
    case sdr_command: reply[0]=1;break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<4;++n)write32(rdram,r.receive+n*4,reply[n]);
    completeNativeCall(rdram,context,r,server);
    ++sound_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sound_rpc_calls==1?0x60002:0x60002+sound_rpc_calls;return true;
}

bool EeRuntime::nativeSoundsCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=256 || r.recv_size!=64)throw UnsupportedSyscall(0x77,context->pc);
    if(on_sound_evidence) {checkMemory(rdram,r.send_source,256);on_sound_evidence("sounds",r.rpc_number,std::span<const std::uint8_t>(memory_.data()+r.send_source,256));}
    switch(r.rpc_number) {
    case sounds_init: break;
    case sounds_bank: {
        // {bank handle, IOP block 1, IOP block 2}: both blocks must have been uploaded to the heap.
        const auto first=read32(rdram,r.send_source+4),second=read32(rdram,r.send_source+8);
        if(!native_heap_.isBlock(first) || !native_heap_.isBlock(second))throw UnsupportedSyscall(0x77,context->pc);
        if(on_sound_evidence) {
            on_sound_evidence("bank-head",first,std::span<const std::uint8_t>(native_iop_ram_.data()+first,native_heap_.size(first)));
            on_sound_evidence("bank-body",second,std::span<const std::uint8_t>(native_iop_ram_.data()+second,native_heap_.size(second)));
        }
        // {sound memory address of the samples, header block, sequence block}.
        try {
            sound_sampler.load(read32(rdram,r.send_source),std::span<const std::uint8_t>(native_iop_ram_.data()+first,native_heap_.size(first)),
                std::span<const std::uint8_t>(native_iop_ram_.data()+second,native_heap_.size(second)));
        } catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        native_sounds_started_=true;++native_sounds_banks_;break;
    }
    case sounds_unload:
        if(!native_sounds_started_ || !native_sounds_banks_ || sound_sampler.banks()!=native_sounds_banks_)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.unload();--native_sounds_banks_;break;
    case sounds_play: {
        // Up to 15 sounds in parallel arrays of 0x20 bytes: byte 0 is the count, then per sound k
        // (from 1) the bank slot at +k, the sound number at 0x20+k, the volume at 0x40+k, the pan at
        // 0x60+k and a 16-bit pitch offset in cents at 0x80+2k.
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,r.send_source,256);
        const auto* command=memory_.data()+r.send_source;
        const unsigned count=command[0];
        if(count>15)throw UnsupportedSyscall(0x77,context->pc);
        try {
            for(unsigned k=1;k<=count;++k) {
                std::int16_t cents;std::memcpy(&cents,command+0x80+k*2,2);
                sound_sampler.play(command[k],command[0x20+k],command[0x40+k],command[0x60+k],cents);
            }
        } catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        break;
    }
    // Not decoded: answered as logged, without an audible effect.
    case sounds_50: case sounds_200: case sounds_60: case sounds_80: case sounds_100: if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<16;++n)write32(rdram,r.receive+n*4,!native_sounds_started_?0u:n==5?native_sounds_banks_:sounds_status[n]);
    completeNativeCall(rdram,context,r,server);
    ++sounds_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sounds_rpc_calls==1?0x60004:0x60004+sounds_rpc_calls;return true;
}
