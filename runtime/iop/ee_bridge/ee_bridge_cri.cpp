#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. CRI ADX requests: the stream bind, creation, submissions and control commands.

void PS2Runtime::receiveCriBind(std::uint8_t* rdram,R5900Context* context) {
    constexpr unsigned client=0x355C70,service=0x90000200;
    const auto d=static_cast<unsigned>(context->r[4].low);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || cri_rpc_binds
        || disc_rpc_calls!=6 || ncmd_read_calls!=1 || context->pc!=0x32F028
        || context->r[29].low!=d || d!=initial_sp-496 || context->r[5].low!=1
        || semaphores.size()!=11 || semaphores[10].count || semaphores[10].maximum!=1
        || semaphores[10].option!=0x3838D8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,4> descriptor={0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,9> packet={64,0,0x80000009,0,5,0x203A97C0,52,client,service};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,client)!=0x203A97C0 || read32(rdram,client+4)!=52 || read32(rdram,client+8)!=10
        || read32(rdram,client+20) || read32(rdram,client+36))throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;const auto& server=rt.rpc_servers.at(service);
    const auto& queue=rt.rpc_queues.at(server.queue);const auto& worker=rt.threads.at(queue.thread);
    if(!worker.waiting || worker.ready || worker.wait_rpc!=server.queue || worker.gp!=server.gp
        || server.handler!=0x1A7720 || server.buffer!=0x1B57C0 || server.descriptor!=0x1B5D30)
        throw UnsupportedSyscall(0x77,context->pc);
    rt.check(server.descriptor,68);rt.check(server.buffer,4);rt.check(server.handler,4);
    const std::array<unsigned,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,client,0x80000009,server.descriptor,server.buffer,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,client,10,server.descriptor,server.buffer);
    ++cri_rpc_binds;++sif_dma_submissions;context->r[2].low=0x780001;
}

void PS2Runtime::receiveCriCreate(std::uint8_t* rdram,R5900Context* context) {
    const bool second=cri_rpc_calls==35 && heap_calls==5;
    const auto d=static_cast<unsigned>(context->r[4].low);
    auto& rt=native_iop->runtime;const auto& server=rt.rpc_servers.at(0x90000200);
    if(iop_epoch!=1 || prior_iop_services_valid || cri_rpc_binds!=1 || (cri_rpc_calls && !second)
        || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-(second?592u:656u) || context->r[5].low!=2
        || semaphores.size()!=11 || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,8> desc={0x39DC80,server.buffer,16,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x1040,server.buffer,0x8000000A,0,5,0x203A97C0,second?91u:54u,0x355C70,2,16,0x39DD80,4,1,server.descriptor};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    std::vector<std::uint8_t> response;
    try {response=native_iop->callCriCreate(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x39DC80,16));}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
    std::copy(response.begin(),response.end(),memory_.begin()+0x39DD80);
    const std::array<unsigned,16> reply={0x400,0x39DD80,0x80000008,0,0,0,0,0x355C70,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x355C70,10,server.descriptor,server.buffer);
    ++cri_rpc_calls;++sif_dma_submissions;context->r[2].low=0x780002;
    native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
}

bool PS2Runtime::isCriStreamSubmission(std::uint8_t* rdram,const R5900Context* context) {
    // A single descriptor at stream object +48 whose slot names a registered stream; other
    // worker submissions (RPC packets) use the existing receivers.
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(context->r[5].low!=1 || d<48 || d%4 || d+16>ram_size)return false;
    const auto slot=read32(rdram,d-44);
    return slot==0x1B58C0 || slot==0x1B5904;
}

void PS2Runtime::receiveCriStream(std::uint8_t* rdram,R5900Context* context) {
    // Raw EE->IOP copy of a CRI stream buffer registered by original create commands 0/1.
    // Attribute 0 requests no IOP notification; IOP consumption and acknowledgement are unmodeled.
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(!native_iop || iop_epoch!=1 || native_iop->cri_create_calls!=2 || context->pc!=0x32F028
        || context->r[5].low!=1 || d<48 || d%4)
        throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,d-48,68);
    struct Pair {std::uint32_t slot,buffer,iop;};
    constexpr std::array<Pair,2> pairs={{{0x1B58C0,0x365BC0,0x14FD40},{0x1B5904,0x35FD00,0x191740}}};
    const auto object=d-48;const auto slot=read32(rdram,object+4);
    const auto pair=std::find_if(pairs.begin(),pairs.end(),[&](const Pair& p){return p.slot==slot;});
    if(pair==pairs.end() || read32(rdram,object+12)!=pair->buffer || read32(rdram,object+24)!=pair->iop
        || read32(rdram,object+28)!=0x880 || (read32(rdram,object)&0xFF00))
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> descriptor={pair->buffer,pair->iop,0x880,0};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,pair->buffer,0x880);native_iop->runtime.check(pair->iop,0x880);
    std::copy_n(memory_.data()+pair->buffer,0x880,native_iop->runtime.ram.data()+pair->iop);
    ++cri_stream_transfers;++sif_dma_submissions;
    context->r[2].low=0x790000+cri_stream_transfers; // Owned logical transfer token.
}

void PS2Runtime::receiveCriControl(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<unsigned>(context->r[4].low);
    auto& rt=native_iop->runtime;const auto& server=rt.rpc_servers.at(0x90000200);
    const bool channel=cri_rpc_calls>=36;
    const bool channel_setup=cri_rpc_calls==36;
    const bool channel_pairs=channel && !channel_setup;
    // Other clients consume packet IDs while the archive loads. Match this
    // request to its live client instead of assuming consecutive CRI IDs.
    const auto rpc_id=channel_pairs?read32(rdram,0x355C74):channel?cri_rpc_calls+57u:cri_rpc_calls>=3?cri_rpc_calls+55u:cri_rpc_calls>=2?56u:55u;
    const bool batch=cri_rpc_calls>=3 && !channel;
    const bool start=batch && !(cri_rpc_calls&1);
    const bool stream=batch && !start;
    const auto input_size=channel && !channel_setup?16u:start?16u:12u;
    const auto output_size=channel_setup?0u:channel || batch?4u:cri_rpc_calls==2?12u:0u;
    const bool payload=cri_rpc_calls>=2;
    if(iop_epoch!=1 || prior_iop_services_valid || cri_rpc_binds!=1 || (cri_rpc_calls<1 || cri_rpc_calls>41 || cri_rpc_calls==35)
        || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-(channel_setup?592u:start?736u:stream?688u:656u) || context->r[5].low!=(payload?2u:1u)
        || semaphores.size()!=11 || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    if(channel_pairs && (!main_priority_admissions || current_thread_id!=1 || startup_scheduler_active
        || main_thread_priority!=1 || !rpc_id || read32(rdram,0x355C70)!=0x203A97C0
        || read32(rdram,0x355C78)!=10 || read32(rdram,0x355C84)!=server.buffer
        || read32(rdram,0x355C94)!=server.descriptor))
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,8> desc={0x39DC80,server.buffer,input_size,0,0x203A97C0,0x1E640,64,0x44};
    const auto descriptor_start=payload?0u:4u;
    for(unsigned n=descriptor_start;n<8;++n)if(read32(rdram,d+(n-descriptor_start)*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    const auto command=channel?(channel_setup?0x40Au:0x408u):start?0x400u:stream?0x422u:payload?0x40Cu:0x403u;
    const std::array<unsigned,14> packet={input_size==16?0x1040u:payload?0xC40u:64u,payload?server.buffer:0u,0x8000000A,0,5,0x203A97C0,rpc_id,0x355C70,command,payload?input_size:0u,0x39DD80,output_size,1,server.descriptor};
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    std::vector<std::uint8_t> response;
    try {response=native_iop->callCriControl(iop_epoch,command,payload?std::span<const std::uint8_t>(memory_.data()+0x39DC80,input_size):std::span<const std::uint8_t>{});}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;throw UnsupportedSyscall(0x77,context->pc);}
    std::copy(response.begin(),response.end(),memory_.begin()+0x39DD80);
    const std::array<unsigned,16> reply={output_size<<8,output_size?0x39DD80u:0u,0x80000008,0,0,0,0,0x355C70,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x355C70,10,server.descriptor,server.buffer);
    ++cri_rpc_calls;++sif_dma_submissions;context->r[2].low=0x780002+cri_rpc_calls-1;
    native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
}
