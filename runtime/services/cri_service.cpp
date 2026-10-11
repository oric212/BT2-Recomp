#include "ee/ps2_runtime.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

// Native CRI ADX service (0x90000200) in place of CRI_ADXI. Replies as the original returned them
// (traced 2026-10-11); the handles are opaque IOP addresses the EE keeps and passes back.
// - 2 create channel {index, EE buffer, IOP buffer, 0x880}: channel handle 0x1B58C0 + index*0x44.
// - 0x403, 0x40A: no reply. 0x40C {0xBB80, 0xFFFF, 0xFFFF}: {0, 1, 0}.
// - 0x422 {1, IOP buffer, 0x100}: handle 0x1BBA40 + n*0x30. 0x400 {..., handle, 1, ...}: 0x1BC580 + n*0x14.
// - 0x408 {2, 0, handle, handle}: 0x1B5DA0 + n*0x1C.
// Stream channels: the EE uploads each channel's 0x880-byte buffer, and at the next VBlank the IOP
// returns it with the last word (the EE's sequence counter) advanced by one. Buffers carrying stream
// commands (any other non-zero word) stop at the next VBlank, where their answer would be needed.

namespace {
constexpr std::uint32_t cri_create=2,cri_403=0x403,cri_40a=0x40A,cri_40c=0x40C,cri_422=0x422,cri_400=0x400,cri_408=0x408;
constexpr std::uint32_t stream_size=0x880;
}

bool PS2Runtime::nativeCriCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    const auto in=[&](unsigned n){if(n*4+4>r.send_size)throw UnsupportedSyscall(0x77,context->pc);return read32(rdram,r.send_source+n*4);};
    const auto reply=[&](std::initializer_list<std::uint32_t> words) {
        if(r.recv_size!=words.size()*4)throw UnsupportedSyscall(0x77,context->pc);
        unsigned n=0;for(auto w:words)write32(rdram,r.receive+4*n++,w);
    };
    const auto none=[&]{if(r.recv_size)throw UnsupportedSyscall(0x77,context->pc);};
    bool create=false;
    switch(r.rpc_number) {
    case cri_create: {
        const auto index=in(0),ee=in(1),iop=in(2),size=in(3);
        if(index>1 || size!=stream_size || native_cri_channels_.size()!=index || iop+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,ee,size);
        native_cri_channels_.push_back({ee,iop,false});
        reply({0x1B58C0+index*0x44});create=true;break;
    }
    case cri_403: none();break;
    case cri_40a: if(r.send_size!=12)throw UnsupportedSyscall(0x77,context->pc);none();break;
    case cri_40c: if(in(0)!=0xBB80)throw UnsupportedSyscall(0x77,context->pc);reply({0,1,0});break;
    case cri_422: if(in(0)!=1)throw UnsupportedSyscall(0x77,context->pc);reply({0x1BBA40+native_cri_422s_++*0x30});break;
    case cri_400: reply({0x1BC580+native_cri_400s_++*0x14});break;
    case cri_408: if(in(0)!=2)throw UnsupportedSyscall(0x77,context->pc);reply({0x1B5DA0+native_cri_408s_++*0x1C});break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    completeNativeCall(rdram,context,r,server);
    ++cri_rpc_calls;++sif_dma_submissions;++native_cri_requests;
    context->r[2].low=create?0x780002:0x780002+cri_rpc_calls-1;return true;
}

bool PS2Runtime::nativeCriStreamUpload(std::uint8_t* rdram,R5900Context* context,std::uint32_t source,std::uint32_t destination,std::uint32_t size) {
    const auto channel=std::find_if(native_cri_channels_.begin(),native_cri_channels_.end(),[&](const auto& c){return c.ee==source && c.iop==destination;});
    if(channel==native_cri_channels_.end())return false;
    if(size!=stream_size)throw UnsupportedSyscall(0x77,context->pc);
    channel->pending=true;
    ++cri_stream_transfers;++sif_dma_submissions;context->r[2].low=0x790000+cri_stream_transfers;
    return true;
}

void PS2Runtime::nativeCriVblank(const R5900Context* interrupted) {
    // The IOP consumes each uploaded buffer and returns it with the sequence counter advanced.
    for(auto& c:native_cri_channels_) {
        if(!c.pending)continue;
        // A buffer carrying stream commands (ADX playback) needs the IOP's answer: not implemented yet.
        if(std::any_of(native_iop_ram_.begin()+c.iop,native_iop_ram_.begin()+c.iop+stream_size-4,[](std::uint8_t b){return b!=0;})) {
            // Name the refused commands: the nonzero words of the buffer (offset: value).
            std::cerr<<"CRI stream commands not answered (channel at IOP 0x"<<std::hex<<c.iop<<"):";
            for(std::size_t k=0;k+4<=stream_size-4;k+=4) {
                std::uint32_t w;std::memcpy(&w,native_iop_ram_.data()+c.iop+k,4);
                if(w)std::cerr<<" +"<<k<<":"<<w;
            }
            std::cerr<<std::dec<<std::endl;
            throw UnsupportedSyscall(0x14,interrupted->pc);
        }
        std::copy_n(native_iop_ram_.begin()+c.iop,stream_size,memory_.begin()+c.ee);
        std::uint32_t counter;std::memcpy(&counter,memory_.data()+c.ee+stream_size-4,4);++counter;
        std::memcpy(memory_.data()+c.ee+stream_size-4,&counter,4);
        c.pending=false;++native_cri_returns;
    }
}
