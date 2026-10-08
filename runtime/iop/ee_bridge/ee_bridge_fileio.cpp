#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. Boot FILEIO requests: the file server bind and calls, forwarded to the native FILEIO handler.

void PS2Runtime::receiveFileioBind(std::uint8_t* rdram,R5900Context* context) {
    // The C library's stdio initialization binds boot FILEIO's file server (0x80000001). Retail
    // placement validated in captured IOP RAM: descriptor 0x47788 (handler 0x46298), buffer 0x477D0.
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const auto rid=read32(rdram,0x3AC744),sema=read32(rdram,0x3AC748);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || fileio_binds || context->pc!=0x32F028 || context->r[29].low!=d
        || context->r[5].low!=1 || !rid || sema>=semaphores.size() || semaphores[sema].count || semaphores[sema].maximum!=1
        || semaphores[sema].option!=0x3838D8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> descriptor={0x203A97C0,0x1E640,64,0x44};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,9> request={64,0,0x80000009,0,5,0x203A97C0,rid,0x3AC740,0x80000001};
    for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC740)!=0x203A97C0 || read32(rdram,0x3AC754) || read32(rdram,0x3AC764))throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC740,0x80000009,0x47788,0x477D0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC740,sema,0x47788,0x477D0);
    ++fileio_binds;++sif_dma_submissions;context->r[2].low=0x70000+fileio_binds;
}

void PS2Runtime::receiveFileioCall(std::uint8_t* rdram,R5900Context* context) {
    // The C library's FILEIO calls: request -> original boot FILEIO handler; reply to the client buffer.
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const auto rid=read32(rdram,0x3AC744),sema=read32(rdram,0x3AC748);
    const auto command=read32(rdram,0x3A97E0),send_size=read32(rdram,0x3A97E4),receive=read32(rdram,0x3A97E8),receive_size=read32(rdram,0x3A97EC);
    const auto source=read32(rdram,d);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || fileio_binds!=1 || context->pc!=0x32F028 || context->r[29].low!=d
        || context->r[5].low!=2 || !rid || sema>=semaphores.size() || semaphores[sema].count || semaphores[sema].maximum!=1
        || semaphores[sema].option!=0x3838E8 || send_size>0x800 || receive_size>0x800 || (send_size && source%16) || (receive_size && receive%16) || local_disc_path.empty())
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> desc={source,0x477D0,send_size,0,0x203A97C0,0x1E640,64,0x44};
    for(unsigned n=send_size?0u:4u;n<8;++n)if(read32(rdram,d+(send_size?n:n-4)*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,14> packet={0,send_size?0x477D0u:0u,0x8000000A,0,5,0x203A97C0,rid,0x3AC740,command,send_size,receive,receive_size,1,0x47788};
    for(unsigned n=1;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC740)!=0x203A97C0 || read32(rdram,0x3AC754)!=0x477D0 || read32(rdram,0x3AC764)!=0x47788)throw UnsupportedSyscall(0x77,context->pc);
    if(send_size)checkMemory(rdram,source,send_size);
    if(receive_size)checkMemory(rdram,receive,receive_size);
    // File operations (open/read/...) run on FILEIO's per-file worker thread, which reports through
    // IOP->EE SIF commands delivered below (EE_FILEIO_PLAN.md).
    if(command!=0xFF && !fileio_workers_supported)throw UnsupportedSyscall(0x77,context->pc);
    std::vector<std::uint8_t> response;auto& rt=native_iop->runtime;
    try {response=native_iop->callFileio(iop_epoch,command,std::span<const std::uint8_t>(memory_.data()+source,send_size),receive_size,memory(),local_disc_path);}
    catch(const std::exception& e) {
        native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
        throw UnsupportedSyscall(0x77,context->pc);
    }
    std::copy(response.begin(),response.end(),memory_.begin()+receive);
    const std::array<std::uint32_t,16> reply={receive_size<<8,receive_size?receive:0u,0x80000008,0,0,0,0,0x3AC740,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC740,sema,0x47788,0x477D0);
    // FILEIO workers report through IOP->EE SIF commands; deliver them to the C library's handler
    // (registered at stdio initialization: 0x80000011 -> 0x333378, argument 0x3AC780).
    auto commands=std::move(native_iop->pending_ee_commands);native_iop->pending_ee_commands.clear();
    for(const auto& command:commands) {
        if(command.cid!=0x80000011)throw UnsupportedSyscall(0x77,context->pc);
        std::copy(command.header.begin(),command.header.end(),memory_.begin()+0x3A94C0);
        deliverEeCommand(rdram,context,0x333378,0x3AC780);++fileio_completions;
    }
    ++fileio_calls;++sif_dma_submissions;context->r[2].low=0x70100+fileio_calls;
    native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
}
