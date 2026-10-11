#include "ee/ps2_runtime.h"
#include <array>

// Native sound driver (SDRDRV, 0x80000701) and SOUNDS (0x02000004) services. Replies as the
// original modules returned them (traced 2026-10-11); audio output itself is a later step.
// - SDRDRV: 64-byte commands, 16-byte replies. 0xE620 initializes (zeros); 0x80D0 transfers a
//   sound block from IOP memory to sound RAM and returns its size (word 5); 0x80F0 returns 1.
// - SOUNDS: 256-byte commands, 64-byte replies. 0x10 initializes (zeros); 0x20 loads a sound bank
//   and starts the worker; 0x40 requests a sound. After 0x20 every reply is the module's status
//   block, whose words 8 and 13 are the worker's IOP thread ID and an IOP address (opaque).

namespace {
constexpr std::uint32_t sdr_init=0xE620,sdr_block_transfer=0x80D0,sdr_command=0x80F0;
constexpr std::uint32_t sounds_init=0x10,sounds_bank=0x20,sounds_play=0x40;
constexpr std::array<std::uint32_t,16> sounds_status={0,0,0,0,1,1,0,0,0x20010,0,0,0,2,0x1C2440,0,0};
}

bool PS2Runtime::nativeSoundDriverCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
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

bool PS2Runtime::nativeSoundsCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=256 || r.recv_size!=64)throw UnsupportedSyscall(0x77,context->pc);
    switch(r.rpc_number) {
    case sounds_init: break;
    case sounds_bank: {
        // {bank handle, IOP block 1, IOP block 2}: both blocks must have been uploaded to the heap.
        const auto first=read32(rdram,r.send_source+4),second=read32(rdram,r.send_source+8);
        if(!native_heap_.isBlock(first) || !native_heap_.isBlock(second))throw UnsupportedSyscall(0x77,context->pc);
        native_sounds_started_=true;break;
    }
    case sounds_play: if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<16;++n)write32(rdram,r.receive+n*4,native_sounds_started_?sounds_status[n]:0u);
    completeNativeCall(rdram,context,r,server);
    ++sounds_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sounds_rpc_calls==1?0x60004:0x60004+sounds_rpc_calls;return true;
}
