#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. Controller requests: the DBCMAN bind and main/port RPCs, forwarded to the native pad services.

void PS2Runtime::receiveDbcBind(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(!native_iop || sounds_rpc_calls!=1)throw UnsupportedSyscall(0x77,context->pc);
    if((dbc_rpc_binds==1 && !dbc_rpc_calls) || (dbc_rpc_binds==4 && dbc_rpc_calls>=1 && dbc_rpc_calls<4)) {
        const auto semaphore=dbc_rpc_calls?8u:7u;const auto command=dbc_rpc_calls>=2?0x80001301u:dbc_rpc_calls?0x80001304u:0x80001363u;const auto id=dbc_rpc_calls>=2?35u+dbc_rpc_calls:dbc_rpc_calls?36u:32u;
        auto& rt=native_iop->runtime;const auto server=rt.rpc_servers.at(0x80001300);
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || d!=initial_sp-(dbc_rpc_calls>=2?656:672) || context->r[29].low!=d
            || context->r[5].low!=2 || semaphores.size()!=semaphore+1 || semaphores[semaphore].count || semaphores[semaphore].maximum!=1 || semaphores[semaphore].option!=0x3838E8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> desc={0x3A04C0,server.buffer,144,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x9040,server.buffer,0x8000000A,0,5,0x203A97C0,id,0x3A03C0,command,144,0x3A04C0,144,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A03C0)!=0x203A97C0 || read32(rdram,0x3A03C4)!=id || read32(rdram,0x3A03C8)!=semaphore
            || read32(rdram,0x3A03D4)!=server.buffer || read32(rdram,0x3A03E4)!=server.descriptor)throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,0x3A04C0,144);std::vector<std::uint8_t> response;
        try {response=native_iop->callDbcRpc(iop_epoch,command,std::span<const std::uint8_t>(memory_.data()+0x3A04C0,144),144,memory_);}
        catch(const std::exception& e) {native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A04C0);
        const std::array<std::uint32_t,16> reply={0x9000,0x3A04C0,0x80000008,0,0,0,0,0x3A03C0,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x3A03C0,semaphore,server.descriptor,server.buffer);
        ++dbc_rpc_calls;++sif_dma_submissions;context->r[2].low=0x70000+dbc_rpc_binds+dbc_rpc_calls;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
    constexpr std::array<std::uint32_t,4> clients={0x3A03C0,0x3A0410,0x3A0438,0x3A0460};
    constexpr std::array<std::uint32_t,4> services={0x80001300,0x8000131C,0x8000131E,0x8000131F};
    if(dbc_rpc_binds>=clients.size())throw UnsupportedSyscall(0x77,context->pc);
    const auto client=clients[dbc_rpc_binds];
    const auto service=services[dbc_rpc_binds];
    const auto id=31u+dbc_rpc_binds+dbc_rpc_calls;
    if(!native_iop || sounds_rpc_calls!=1 || dbc_rpc_binds>=4 || (dbc_rpc_binds && dbc_rpc_calls!=1) || iop_epoch!=1 || prior_iop_services_valid
        || context->pc!=0x32F028 || d!=initial_sp-544 || context->r[29].low!=d || context->r[5].low!=1
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838D8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,id,client,service};
    for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+4)!=id || read32(rdram,client+8)!=7
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
    deliverRpcEnd(rdram,context,client,7,server.descriptor,server.buffer);
    ++dbc_rpc_binds;++sif_dma_submissions;context->r[2].low=0x70000+dbc_rpc_binds+dbc_rpc_calls;
}

void PS2Runtime::receiveDbcMain(std::uint8_t* rdram,R5900Context* context) {
    // Frame-phase blocking requests on DBCMAN's main client (service 0x80001300) in DBCMAN's command
    // range (0x800013xx), e.g. the socket attach (0x80001303) and the device command (0x8000131A) the
    // game issues once a controller is present. The original registered IOP
    // handler produces the 144-byte reply; the call's wait semaphore is the client's own.
    constexpr std::uint32_t client=0x3A03C0,buffer=0x3A04C0;
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const auto command=read32(rdram,0x3A97E0),rid=read32(rdram,client+4),sema=read32(rdram,client+8);
    auto refuse=[&](const char* why){std::cerr<<"DBCMAN main request refused: "<<why<<" (command "<<command<<")"<<std::endl;throw UnsupportedSyscall(0x77,context->pc);};
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || dbc_rpc_binds!=4 || dbc_rpc_calls!=4)refuse("service state");
    if(current_thread_id!=1 || startup_scheduler_active || in_interrupt_ || !frameGifContext())refuse("thread/phase");
    if(context->pc!=0x32F028 || context->r[29].low!=d || context->r[5].low!=2 || (command&0xFFFFFF00u)!=0x80001300u)refuse("call shape");
    if(sema>=semaphores.size() || semaphores[sema].count || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8)refuse("wait semaphore");
    auto& rt=native_iop->runtime;const auto& server=rt.rpc_servers.at(0x80001300);
    const std::array<std::uint32_t,8> desc={buffer,server.buffer,144,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x9040,server.buffer,0x8000000A,0,5,0x203A97C0,rid,client,command,144,buffer,144,1,server.descriptor};
    for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])refuse("descriptor");
    for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])refuse("packet");
    if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+20)!=server.buffer || read32(rdram,client+36)!=server.descriptor)refuse("client");
    checkMemory(rdram,buffer,144);std::vector<std::uint8_t> response;
    try {response=native_iop->callDbcMain(iop_epoch,command,std::span<const std::uint8_t>(memory_.data()+buffer,144));}
    catch(const std::exception& e) {native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
    std::copy(response.begin(),response.end(),memory_.begin()+buffer);
    const std::array<std::uint32_t,16> reply={0x9000,buffer,0x80000008,0,0,0,0,client,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,client,sema,server.descriptor,server.buffer);
    ++dbc_main_calls;++sif_dma_submissions;context->r[2].low=0x7C0000+dbc_main_calls; // Owned logical transfer token.
    native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
}

void PS2Runtime::receiveDbcPort(std::uint8_t* rdram,R5900Context* context) {
    // Main's frame-phase no-wait calls on the third/fourth DBC clients: 144 bytes out of and back
    // into the client's buffer, no end function. The original IOP handler produces the reply; RPC_END is delivered
    // at once in logical time (the EE later checks the client's completion itself).
    struct Port {std::uint32_t client,service,buffer;};
    constexpr std::array<Port,2> ports={{{0x3A0438,0x8000131E,0x3A2640},{0x3A0460,0x8000131F,0x3A2700}}};
    const auto port=std::find_if(ports.begin(),ports.end(),[&](const Port& p){return p.client==read32(rdram,0x3A97DC);});
    if(port==ports.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto client=port->client,service=port->service,buffer=port->buffer,command=read32(rdram,0x3A97E0);
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || dbc_rpc_binds!=4 || dbc_rpc_calls!=4
        || current_thread_id!=1 || startup_scheduler_active || in_interrupt_ || !frameGifContext()
        || context->pc!=0x32F028 || context->r[29].low!=d || context->r[5].low!=2)
        throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(service);
    if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto server=found->second;const auto rid=read32(rdram,client+4);
    const std::array<std::uint32_t,8> desc={buffer,server.buffer,144,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x9040,server.buffer,0x8000000A,0,5,0x203A97C0,rid,client,command,144,buffer,144,0,server.descriptor};
    for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+8)!=UINT32_MAX || read32(rdram,client+28)
        || read32(rdram,client+20)!=server.buffer || read32(rdram,client+36)!=server.descriptor)throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,buffer,144);std::vector<std::uint8_t> response;
    try {response=native_iop->callDbcPort(iop_epoch,service,client,buffer,command,std::span<const std::uint8_t>(memory_.data()+buffer,144),144);}
    catch(const std::exception& e) {native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
    std::copy(response.begin(),response.end(),memory_.begin()+buffer);
    const std::array<std::uint32_t,16> reply={0x9000,buffer,0x80000008,0,0,0,0,client,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,client,0,server.descriptor,server.buffer);
    ++dbc_port_calls;++sif_dma_submissions;context->r[2].low=0x7B0000+dbc_port_calls; // Owned logical transfer token.
    native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
}
