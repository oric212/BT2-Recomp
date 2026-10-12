#include "ee/ee_runtime.h"
#include <array>

// Native memory card service (MCSERV, 0x80000400) with no card inserted. Requests use the public
// mcDescParam layout (48 bytes: port, slot, flags, type/free/format request flags, buffer, EE end
// parameter address). Results as MCSERV returned them for an empty port 0 / slot 0:
// - init (0xFE): {0, server version 0x210, driver version 0x226};
// - GetInfo (1, no-wait): reply -11 (no card); the 192-byte end parameter receives type 0 and free
//   space 0, after which the game's end function runs. The original's remaining end-parameter bytes
//   are stale IOP buffer content and are zero here.

namespace {
constexpr std::uint32_t mc_get_info=1,mc_init=0xFE;
constexpr std::uint32_t mc_no_card=static_cast<std::uint32_t>(-11);
constexpr unsigned mc_info_size=192;
}

bool EeRuntime::nativeCardCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=48)throw UnsupportedSyscall(0x77,context->pc);
    std::array<std::uint32_t,12> in;for(unsigned n=0;n<12;++n)in[n]=read32(rdram,r.send_source+n*4);
    if(r.rpc_number==mc_init) {
        if(r.recv_size!=12)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,0);write32(rdram,r.receive+4,0x210);write32(rdram,r.receive+8,0x226);
        completeNativeCall(rdram,context,r,server);
        ++mc_rpc_calls;++sif_dma_submissions;++native_card_requests;context->r[2].low=0x720002;return true;
    }
    if(r.rpc_number==mc_get_info) {
        // Port 0 / slot 0 only; the end parameter address must match the client's end parameter.
        const auto info=in[7];
        if(r.recv_size!=4 || in[0] || in[1] || info!=read32(rdram,r.client+32))throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,info,mc_info_size);
        for(unsigned n=0;n<mc_info_size;n+=4)write32(rdram,info+n,0); // Type 0, free 0.
        write32(rdram,r.receive,mc_no_card);
        completeNativeCall(rdram,context,r,server);
        ++sif_dma_submissions;++native_card_requests;
        if(mc_rpc_calls==1){++mc_rpc_calls;context->r[2].low=0x720003;}
        else {++mc_frame_queries;context->r[2].low=0x750000+mc_frame_queries;}
        return true;
    }
    throw UnsupportedSyscall(0x77,context->pc);
}
