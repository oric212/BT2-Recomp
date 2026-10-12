#include "ee/ee_runtime.h"
#include <array>

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
    switch(r.rpc_number) {
    case sdr_init: break;
    case sdr_block_transfer: {
        // {reply buffer, 1, 0, IOP source, sound RAM address, size, ...}: the source must be uploaded data.
        const auto source=read32(rdram,r.send_source+12),size=read32(rdram,r.send_source+20);
        if(!size || source+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
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
    switch(r.rpc_number) {
    case sounds_init: break;
    case sounds_bank: {
        // {bank handle, IOP block 1, IOP block 2}: both blocks must have been uploaded to the heap.
        const auto first=read32(rdram,r.send_source+4),second=read32(rdram,r.send_source+8);
        if(!native_heap_.isBlock(first) || !native_heap_.isBlock(second))throw UnsupportedSyscall(0x77,context->pc);
        native_sounds_started_=true;++native_sounds_banks_;break;
    }
    case sounds_unload:
        if(!native_sounds_started_ || !native_sounds_banks_)throw UnsupportedSyscall(0x77,context->pc);
        --native_sounds_banks_;break;
    case sounds_play: case sounds_50: case sounds_200: case sounds_60: case sounds_80: case sounds_100: if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<16;++n)write32(rdram,r.receive+n*4,!native_sounds_started_?0u:n==5?native_sounds_banks_:sounds_status[n]);
    completeNativeCall(rdram,context,r,server);
    ++sounds_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sounds_rpc_calls==1?0x60004:0x60004+sounds_rpc_calls;return true;
}
