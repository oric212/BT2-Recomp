#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. Memory card requests: the MCSERV RPC, startup and frame-phase card queries.

void PS2Runtime::receiveMcRpc(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    constexpr auto client=0x3A6F80u,service=0x80000400u;
    const auto id=mc_rpc_binds?40u:39u;
    if(mc_rpc_binds==1 && mc_rpc_calls==2) {
        // Later no-wait GetInfo queries (frame phase): same request layout as the first query; the
        // request id, caller stack and semaphore population are the game's current state.
        if(!native_iop || dbc_rpc_calls!=4 || iop_epoch!=1 || context->pc!=0x32F028
            || context->r[29].low!=d || context->r[5].low!=2)throw UnsupportedSyscall(0x77,context->pc);
        auto& rt=native_iop->runtime;const auto server=rt.rpc_servers.at(service);
        const auto rid=read32(rdram,client+4);
        const std::array<std::uint32_t,8> desc={0x3A7000,server.buffer,48,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x3040,server.buffer,0x8000000A,0,5,0x203A97C0,rid,client,1,48,0x3A8540,4,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+8)!=UINT32_MAX
            || read32(rdram,client+28)!=0x31DD60 || read32(rdram,client+32)!=0x3A7480
            || read32(rdram,client+20)!=server.buffer || read32(rdram,client+36)!=server.descriptor)throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callMcQuery(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3A7000,48),memory());}
        catch(const std::exception& e){native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A8540);
        const std::array<std::uint32_t,16> reply={0x400,0x3A8540,0x80000008,0,0,0,0,client,0x8000000A};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
        deliverRpcEnd(rdram,context,client,0,server.descriptor,server.buffer);
        ++mc_frame_queries;++sif_dma_submissions;context->r[2].low=0x750000+mc_frame_queries;return; // Owned logical transfer token.
    }
    if(mc_rpc_binds==1 && mc_rpc_calls==1) {
        if(!native_iop || dbc_rpc_calls!=4 || iop_epoch!=1 || context->pc!=0x32F028 || d!=initial_sp-560
            || context->r[29].low!=d || context->r[5].low!=2 || semaphores.size()!=10
            || semaphores[8].count || semaphores[8].option!=0x3824C8)throw UnsupportedSyscall(0x77,context->pc);
        auto& rt=native_iop->runtime;const auto server=rt.rpc_servers.at(service);
        const std::array<std::uint32_t,8> desc={0x3A7000,server.buffer,48,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x3040,server.buffer,0x8000000A,0,5,0x203A97C0,41,client,1,48,0x3A8540,4,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+4)!=41 || read32(rdram,client+8)!=UINT32_MAX
            || read32(rdram,client+28)!=0x31DD60 || read32(rdram,client+32)!=0x3A7480
            || read32(rdram,client+20)!=server.buffer || read32(rdram,client+36)!=server.descriptor)throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callMcQuery(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3A7000,48),memory());}
        catch(const std::exception& e){native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A8540);
        const std::array<std::uint32_t,16> reply={0x400,0x3A8540,0x80000008,0,0,0,0,client,0x8000000A};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
        deliverRpcEnd(rdram,context,client,0,server.descriptor,server.buffer);
        ++mc_rpc_calls;++sif_dma_submissions;context->r[2].low=0x720003;return;
    }
    if(mc_rpc_binds==1 && !mc_rpc_calls) {
        if(!native_iop || dbc_rpc_calls!=4 || iop_epoch!=1 || context->pc!=0x32F028 || d!=initial_sp-544
            || context->r[29].low!=d || context->r[5].low!=2 || semaphores.size()!=11
            || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838E8)
            throw UnsupportedSyscall(0x77,context->pc);
        auto& rt=native_iop->runtime;const auto server=rt.rpc_servers.at(service);
        const std::array<std::uint32_t,8> desc={0x3A7000,server.buffer,48,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x3040,server.buffer,0x8000000A,0,5,0x203A97C0,id,client,0xFE,48,0x3A8540,12,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+4)!=id || read32(rdram,client+8)!=10
            || read32(rdram,client+20)!=server.buffer || read32(rdram,client+36)!=server.descriptor)throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callMcRpc(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3A7000,48));}
        catch(const std::exception& e){native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A8540);
        const std::array<std::uint32_t,16> reply={0xC00,0x3A8540,0x80000008,0,0,0,0,client,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,client,10,server.descriptor,server.buffer);
        ++mc_rpc_calls;++sif_dma_submissions;context->r[2].low=0x720002;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
    if(!native_iop || dbc_rpc_calls!=4 || mc_rpc_binds || iop_epoch!=1 || prior_iop_services_valid
        || context->pc!=0x32F028 || d!=initial_sp-464 || context->r[29].low!=d || context->r[5].low!=1
        || semaphores.size()!=11 || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838D8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,id,client,service};
    for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+4)!=id || read32(rdram,client+8)!=10
        || read32(rdram,client+20) || read32(rdram,client+36))throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(service);
    if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto server=found->second;const auto queue=rt.rpc_queues.find(server.queue);
    if(queue==rt.rpc_queues.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto worker=rt.threads.find(queue->second.thread);
    if(worker==rt.threads.end() || !worker->second.waiting || worker->second.wait_rpc!=server.queue || worker->second.gp!=server.gp)
        throw UnsupportedSyscall(0x77,context->pc);
    rt.check(server.descriptor,68);rt.check(server.handler,4);rt.check(server.buffer,4);
    const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,client,0x80000009,server.descriptor,server.buffer,0,0,0,0,0};
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,client,10,server.descriptor,server.buffer);
    ++mc_rpc_binds;++sif_dma_submissions;context->r[2].low=0x70000+mc_rpc_binds+mc_rpc_calls;
}
