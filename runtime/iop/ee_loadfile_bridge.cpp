#include "ps2_runtime.h"
#include "module_service.h"
#include <algorithm>
void PS2Runtime::receiveNativeModule(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<std::uint32_t>(context->r[4].low);
        const bool buffer=read32(rdram,0x3A97E0)==6;
        const bool reload=buffer && embeddedReload() && native_module_loads==14 && heap_dma_polls==2 && heap_calls==7;
        const unsigned sema=reload?10u:7u;
        if(context->r[29].low!=d || d!=initial_sp-(buffer?640:528) || context->r[5].low!=2 || semaphores.size()!=sema+1
            || semaphores[sema].count || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8 || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> descriptor={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        const auto id=read32(rdram,0x3ACC04);
        const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,id,0x3ACC00,buffer?6u:0u,512,0x3ACA00,8,1,0x22568};
        for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(!id || read32(rdram,0x3ACC00)!=0x203A97C0 || read32(rdram,0x3ACC08)!=sema || read32(rdram,0x3ACA00)!=(reload?0x196100u:buffer?0xCE500u:0u))
            throw UnsupportedSyscall(0x77,context->pc);
        std::string path;
        if(buffer) {
            if(!native_iop || (!reload && (native_module_loads!=13 || heap_dma_polls!=1)) || read32(rdram,0x3ACA04))throw UnsupportedSyscall(0x77,context->pc);
            // With arg_len zero, the caller's unused argument/path residue is not consumed.
            path="embedded:ck01";
        }else for(unsigned n=0;n<252;++n) {const auto ch=memory_.at(0x3ACA08+n);if(!ch)break;if(ch<32 || ch>126)throw UnsupportedSyscall(0x77,context->pc);path+=static_cast<char>(ch);}
        native_module_request=path;
        if(path.empty() || path.size()==252)throw UnsupportedSyscall(0x77,context->pc);
        if(!native_iop) {native_iop=std::make_shared<NativeIopService>(iop_epoch,bt2_compiled_iop_modules());native_iop->attachEeTransport(iop_epoch); }
        if(!buffer && !native_iop->supports(path))throw UnsupportedSyscall(0x77,context->pc);
        NativeIopService::Resident resident{};
        try {resident=buffer?native_iop->loadBootBuffer(iop_epoch,reload?0x196100u:0xCE500u):native_iop->load(iop_epoch,path);}
        catch(const std::exception& e) {
            native_iop_error=e.what();native_iop_pc=native_iop->runtime.executing_pc;
            native_iop_steps=native_iop->runtime.steps;native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());
            native_iop_waiters=0;
            for(const auto& [thread_id,thread]:native_iop->runtime.threads)if(thread.waiting)++native_iop_waiters;
            throw UnsupportedSyscall(0x77,context->pc);
        }
        native_iop_pc=native_iop->runtime.executing_pc;native_iop_steps=native_iop->runtime.steps;
        native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());native_iop_waiters=0;
        for(const auto& [thread_id,thread]:native_iop->runtime.threads)if(thread.waiting)++native_iop_waiters;
        write32(rdram,0x3ACA00,resident.id);write32(rdram,0x3ACA04,resident.result);
        const std::array<std::uint32_t,16> reply={0x800,0x3ACA00,0x80000008,0,0,0,0,0x3ACC00,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x3ACC00,sema,0x22568,0x225B0);
        ++native_module_loads;++sif_dma_submissions;context->r[2].low=0x30000+serviceOperations();
    }

void PS2Runtime::receiveHeapBind(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<std::uint32_t>(context->r[4].low);
        if(!heap_bind_supplied || heap_binds || native_module_loads!=13 || iop_epoch!=1 || prior_iop_services_valid
            || context->r[29].low!=d || d!=initial_sp-336 || context->r[5].low!=1 || semaphores.size()!=8
            || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838D8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> descriptor={0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,9> request={64,0,0x80000009,0,5,0x203A97C0,22,0x3AC840,0x80000003};
        for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3AC840)!=0x203A97C0 || read32(rdram,0x3AC844)!=22 || read32(rdram,0x3AC848)!=7
            || read32(rdram,0x3AC854) || read32(rdram,0x3AC864))throw UnsupportedSyscall(0x77,context->pc);
        // Reference-backed boot heap service lookup only; no FILEIO worker/request execution claimed.
        const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0};
        for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x3AC840,7,0x48438,0x48480);
        ++heap_binds;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds;
    }

void PS2Runtime::receiveHeapAllocation(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const bool second=heap_calls==1 && heap_frees==1 && texture_gif.phase==8;
    const bool third=heap_calls==2 && cri_rpc_binds==1 && disc_rpc_calls==6 && ncmd_read_calls==1;
    const bool fourth=heap_calls==3 && cri_rpc_calls==3 && disc_rpc_calls==6 && ncmd_read_calls==1;
    const bool fifth=heap_calls==4 && cri_rpc_calls==35 && disc_rpc_calls==6 && ncmd_read_calls==1;
    const bool sixth=heap_calls==5 && cri_rpc_calls==36 && disc_rpc_calls==6 && ncmd_read_calls==1;
    const bool seventh=heap_calls==6 && embeddedReload();
    const auto semaphore=third || fourth || fifth || sixth || seventh?10u:7u;
    // Other clients consume packet IDs before the reload; match the live heap client ID.
    const auto rid=seventh?read32(rdram,0x3AC844):sixth?92u:fifth?90u:fourth?57u:third?53u:second?26u:23u;
    if(!heap_allocation_supplied || (!second && !third && !fourth && !fifth && !sixth && !seventh && heap_calls) || heap_binds!=1 || native_module_loads!=(second || third || fourth || fifth || sixth || seventh?14u:13u) || !rid || !native_iop || iop_epoch!=1
        || prior_iop_services_valid || context->r[29].low!=d || d!=initial_sp-(seventh?528:sixth?560:fifth?560:fourth?640:third?624:second?432:528) || context->r[5].low!=2
        || semaphores.size()!=semaphore+1 || semaphores[semaphore].count || semaphores[semaphore].maximum!=1 || semaphores[semaphore].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> desc={0x3AC8C0,0x48480,4,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x440,0x48480,0x8000000A,0,5,0x203A97C0,rid,0x3AC840,1,4,0x3AC880,4,1,0x48438};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC8C0)!=(seventh?0x360u:sixth?0x4040u:fifth?0x8D0u:fourth?0x41040u:third?0x8D0u:second?0x81800u:0x360u) || read32(rdram,0x3AC840)!=0x203A97C0 || read32(rdram,0x3AC844)!=rid
        || read32(rdram,0x3AC848)!=semaphore || read32(rdram,0x3AC854)!=0x48480 || read32(rdram,0x3AC864)!=0x48438)
        throw UnsupportedSyscall(0x77,context->pc);
    const auto allocated=native_iop->allocateBootHeap(iop_epoch,read32(rdram,0x3AC8C0));
    write32(rdram,0x3AC880,allocated);
    const std::array<std::uint32_t,16> reply={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC840,semaphore,0x48438,0x48480);
    ++heap_calls;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls;
    native_iop_steps=native_iop->runtime.steps;native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());native_iop_pc=native_iop->runtime.executing_pc;
}

void PS2Runtime::receiveHeapCopy(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const bool reload=embeddedReload() && heap_transfers==1 && heap_calls==7;
    if(!heap_copy_supplied || (!reload && (heap_transfers || heap_calls!=1)) || heap_binds!=1 || !native_iop || iop_epoch!=1
        || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-144 || context->r[5].low!=1)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> descriptor={0x369F90,reload?0x196100u:0xCE500u,0x360,0};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    if(native_iop->boot_heap_allocations!=(reload?7u:1u))throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,descriptor[0],descriptor[2]);native_iop->runtime.check(descriptor[1],descriptor[2]);
    std::copy_n(memory_.data()+descriptor[0],descriptor[2],native_iop->runtime.ram.data()+descriptor[1]);
    ++heap_transfers;++sif_dma_submissions;context->r[2].low=0x50000+heap_transfers; // Owned logical transfer token.
}

void PS2Runtime::receiveHeapFree(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    const bool reload=embeddedReload() && native_module_loads==15 && heap_calls==7 && heap_frees==1;
    const unsigned sema=reload?10u:7u;const auto pointer=reload?0x196100u:0xCE500u;
    const auto rid=reload?read32(rdram,0x3AC844):25u;
    if(!native_iop || (!reload && (native_module_loads!=14 || heap_calls!=1 || heap_frees)) || iop_epoch!=1 || prior_iop_services_valid || !rid
        || context->r[29].low!=d || d!=initial_sp-544 || context->r[5].low!=2 || semaphores.size()!=sema+1
        || semaphores[sema].count || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> desc={0x3AC8C0,0x48480,4,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x440,0x48480,0x8000000A,0,5,0x203A97C0,rid,0x3AC840,2,4,0x3AC880,4,1,0x48438};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC8C0)!=pointer || read32(rdram,0x3AC840)!=0x203A97C0 || read32(rdram,0x3AC844)!=rid
        || read32(rdram,0x3AC848)!=sema || read32(rdram,0x3AC854)!=0x48480 || read32(rdram,0x3AC864)!=0x48438)
        throw UnsupportedSyscall(0x77,context->pc);
    write32(rdram,0x3AC880,native_iop->freeBootHeap(iop_epoch,pointer));
    const std::array<std::uint32_t,16> reply={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC840,sema,0x48438,0x48480);
    ++heap_frees;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls+heap_frees;
    native_iop_steps=native_iop->runtime.steps;native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());native_iop_pc=native_iop->runtime.executing_pc;
}


void PS2Runtime::receiveSoundRpc(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(sounds_rpc_binds==1 && !sounds_rpc_calls && native_iop && read32(rdram,0x3A97DC)==0x6351C0) {
        auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(0x02000004);
        if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto server=found->second;
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || d!=initial_sp-416 || context->r[29].low!=d
            || context->r[5].low!=2 || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> desc={0x3B0E00,server.buffer,256,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x10040,server.buffer,0x8000000A,0,5,0x203A97C0,30,0x6351C0,16,256,0x635180,64,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x6351C0)!=0x203A97C0 || read32(rdram,0x6351C4)!=30 || read32(rdram,0x6351C8)!=7
            || read32(rdram,0x6351E4)!=server.descriptor || read32(rdram,0x6351D4)!=server.buffer)
            throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,0x3B0E00,256);checkMemory(rdram,0x635180,64);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callSoundRpc(iop_epoch,16,std::span<const std::uint8_t>(memory_.data()+0x3B0E00,256),64,0x02000004);}
        catch(const std::exception& e) {
            native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
            throw UnsupportedSyscall(0x77,context->pc);
        }
        std::copy(response.begin(),response.end(),memory_.begin()+0x635180);
        const std::array<std::uint32_t,16> reply={0x4000,0x635180,0x80000008,0,0,0,0,0x6351C0,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x6351C0,7,server.descriptor,server.buffer);
        ++sounds_rpc_calls;++sif_dma_submissions;context->r[2].low=0x60004;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
    if(sound_thread_starts==1 && !sounds_rpc_binds && native_iop && read32(rdram,0x3A97DC)==0x6351C0) {
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || d!=initial_sp-320 || context->r[29].low!=d
            || context->r[5].low!=1 || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838D8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,29,0x6351C0,0x02000004};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x6351C0)!=0x203A97C0 || read32(rdram,0x6351C4)!=29 || read32(rdram,0x6351C8)!=7
            || read32(rdram,0x6351E4) || read32(rdram,0x6351D4))throw UnsupportedSyscall(0x77,context->pc);
        auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(0x02000004);
        if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto server=found->second;const auto queue=rt.rpc_queues.find(server.queue);
        if(queue==rt.rpc_queues.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto worker=rt.threads.find(queue->second.thread);
        if(worker==rt.threads.end() || !worker->second.waiting || worker->second.wait_rpc!=server.queue)
            throw UnsupportedSyscall(0x77,context->pc);
        rt.check(server.descriptor,68);rt.check(server.handler,4);rt.check(server.buffer,4);
        const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x6351C0,0x80000009,server.descriptor,server.buffer,0,0,0,0,0};
        if(ee_queued_sound_rpc) {queueSoundsBind(context,reply);return;}
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x6351C0,7,server.descriptor,server.buffer);
        ++sounds_rpc_binds;++sif_dma_submissions;context->r[2].low=0x60003;return;
    }
    if(sound_rpc_binds==1 && !sound_rpc_calls && native_iop) {
        auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(0x80000701);
        if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto server=found->second;
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || d!=initial_sp-736 || context->r[29].low!=d
            || context->r[5].low!=2 || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> desc={0x3A5C40,server.buffer,64,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x4040,server.buffer,0x8000000A,0,5,0x203A97C0,28,0x3A5C80,0xE620,64,0x3A5C40,16,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A5C80)!=0x203A97C0 || read32(rdram,0x3A5C84)!=28 || read32(rdram,0x3A5C88)!=7
            || read32(rdram,0x3A5CA4)!=server.descriptor || read32(rdram,0x3A5C94)!=server.buffer || read32(rdram,0x3A5CA0)!=0x3A5C40)
            throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callSoundRpc(iop_epoch,0xE620,std::span<const std::uint8_t>(memory_.data()+0x3A5C40,64),16);}
        catch(const std::exception& e) {
            native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
            throw UnsupportedSyscall(0x77,context->pc);
        }
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A5C40);
        const std::array<std::uint32_t,16> reply={0x1000,0x3A5C40,0x80000008,0,0,0,0,0x3A5C80,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x3A5C80,7,server.descriptor,server.buffer);
        ++sound_rpc_calls;++sif_dma_submissions;context->r[2].low=0x60002;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || heap_calls!=2 || heap_frees!=1 || sound_rpc_binds
        || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-368 || context->r[5].low!=1
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838D8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,27,0x3A5C80,0x80000701};
    for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3A5C80)!=0x203A97C0 || read32(rdram,0x3A5C84)!=27 || read32(rdram,0x3A5C88)!=7
        || read32(rdram,0x3A5CA4) || read32(rdram,0x3A5C94))throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;
    const auto found=rt.rpc_servers.find(0x80000701);
    if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto& server=found->second;const auto queue=rt.rpc_queues.find(server.queue);
    if(queue==rt.rpc_queues.end())throw UnsupportedSyscall(0x77,context->pc);
    const auto worker=rt.threads.find(queue->second.thread);
    if(worker==rt.threads.end() || !worker->second.waiting || worker->second.wait_rpc!=server.queue)
        throw UnsupportedSyscall(0x77,context->pc);
    rt.check(server.descriptor,68);rt.check(server.handler,4);rt.check(server.buffer,4);
    // Return addresses of the actual owned registration; never retail pointer aliases.
    const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3A5C80,0x80000009,server.descriptor,server.buffer,0,0,0,0,0};
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3A5C80,7,server.descriptor,server.buffer);
    ++sound_rpc_binds;++sif_dma_submissions;context->r[2].low=0x60001;
}

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

void PS2Runtime::receiveMcRpc(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    constexpr auto client=0x3A6F80u,service=0x80000400u;
    const auto id=mc_rpc_binds?40u:39u;
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
        try {response=native_iop->callMcQuery(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3A7000,48),memory_);}
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

void PS2Runtime::receiveDiscSearch(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<unsigned>(context->r[4].low);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-(disc_rpc_calls?832:816) || context->r[5].low!=2
        || semaphores.size()!=11 || semaphores[10].count || semaphores[10].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,8> descriptor={0x3A8600,0x441C8,300,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x12C40,0x441C8,0x8000000A,0,5,0x203A97C0,disc_rpc_calls?46u+disc_rpc_calls:43u,0x3A8780,0,300,0x3A8740,4,1,0x433D0};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3A8788)!=10 || read32(rdram,0x3A8794)!=0x441C8 || read32(rdram,0x3A87A4)!=0x433D0)throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;unsigned result;
    try{result=native_iop->callDiscSearch(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3A8600,300),memory_,local_disc_path);}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());throw UnsupportedSyscall(0x77,context->pc);}
    write32(rdram,0x3A8740,result);
    const std::array<unsigned,16> reply={0x400,0x3A8740,0x80000008,0,0,0,0,0x3A8780,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3A8780,10,0x433D0,0x441C8);++disc_rpc_calls;++sif_dma_submissions;context->r[2].low=0x740002;
    native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());
}

void PS2Runtime::receiveNcmdReady(std::uint8_t* rdram,R5900Context* context) {
    // First call: main before the directory read. Repeat: a dispatched helper after the read,
    // with the client's current RPC ID. Both execute the original IOP NCMD 14 handler.
    const auto d=static_cast<unsigned>(context->r[4].low);
    const bool repeat=ncmd_ready_calls>=1;
    const auto rid=read32(rdram,0x3A97D8);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d
        || (repeat ? !(startup_scheduler_active && current_thread_id>=4 && current_thread_id<=7) || !ncmd_read_calls
                     || rid!=read32(rdram,0x369594) || read32(rdram,0x3A97C0)!=0x40
                   : d!=initial_sp-768 || rid!=45)
        || context->r[5].low!=1 || semaphores.size()<11 || semaphores[10].count
        || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x40,0,0x8000000A,0,5,0x203A97C0,rid,0x369590,14,0,0x368440,4,1,0x43418};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x369598)!=10 || read32(rdram,0x3695A4)!=0x43DB8 || read32(rdram,0x3695B4)!=0x43418)throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;unsigned result;
    try{result=native_iop->callDiscReady(iop_epoch);}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());throw UnsupportedSyscall(0x77,context->pc);}
    write32(rdram,0x368440,result);
    const std::array<unsigned,16> reply={0x400,0x368440,0x80000008,0,0,0,0,0x369590,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x369590,10,0x43418,0x43DB8);++ncmd_ready_calls;++sif_dma_submissions;
    context->r[2].low=0x760001+ncmd_ready_calls; // Owned logical transfer token.
    native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());
}

void PS2Runtime::receiveScmdCall(std::uint8_t* rdram,R5900Context* context) {
    // Helper S-command call after main's admitted priority: no send data, 4-byte result.
    const auto d=static_cast<unsigned>(context->r[4].low);
    const auto rid=read32(rdram,0x3A97D8),fn=read32(rdram,0x3A97E0),sema=read32(rdram,0x369F48);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d
        || !(startup_scheduler_active && current_thread_id>=4 && current_thread_id<=7) || context->r[5].low!=1
        || rid!=read32(rdram,0x369F44) || sema>=semaphores.size() || semaphores[sema].count || semaphores[sema].maximum!=1
        || semaphores[sema].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x40,0,0x8000000A,0,5,0x203A97C0,rid,0x369F40,fn,0,0x3695C0,4,1,0x434F0};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x369F54)!=0x439A8 || read32(rdram,0x369F64)!=0x434F0)throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;unsigned result;
    try{result=native_iop->callDiscScmd(iop_epoch,fn);}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());throw UnsupportedSyscall(0x77,context->pc);}
    write32(rdram,0x3695C0,result);
    const std::array<unsigned,16> reply={0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x369F40,sema,0x434F0,0x439A8);++scmd_calls;++sif_dma_submissions;
    context->r[2].low=0x7A0000+scmd_calls; // Owned logical transfer token.
    native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());
}

void PS2Runtime::receiveNcmdRead(std::uint8_t* rdram,R5900Context* context) {
    // First read: main's directory sector. Repeat: a dispatched helper after a readiness query,
    // with the client's current RPC ID. Both are no-wait calls completed by the original callback.
    const auto d=static_cast<unsigned>(context->r[4].low);
    const bool repeat=ncmd_read_calls>=1;
    const auto rid=read32(rdram,0x3A97D8);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d
        || (repeat ? !(startup_scheduler_active && current_thread_id>=4 && current_thread_id<=7) || ncmd_ready_calls<=ncmd_read_calls
                   : d!=initial_sp-720 || semaphores.size()!=10 || rid!=46)
        || context->r[5].low!=2)throw UnsupportedSyscall(0x77,context->pc);
    // More than 16 sectors streams through the FSV ring in chunks (sceCdRV cache count 16);
    // that CDVDMAN streaming protocol is not modelled. Stop before the IOP is entered.
    if(read32(rdram,0x3684C4)>16)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,8> desc={0x3684C0,0x43DB8,24,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x1840,0x43DB8,0x8000000A,0,5,0x203A97C0,rid,0x369590,1,24,0,0,1,0x43418};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,10> client={0x203A97C0,rid,UINT32_MAX,0,0,0x43DB8,0x38BDF0,0x31ECF8,0x3694C0,0x43418};
    for(unsigned n=0;n<10;++n)if(read32(rdram,0x369590+n*4)!=client[n])throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;
    try{native_iop->callDiscRead(iop_epoch,std::span<const std::uint8_t>(memory_.data()+0x3684C0,24),memory_,local_disc_path);}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());throw UnsupportedSyscall(0x77,context->pc);}
    const std::array<unsigned,16> reply={0,0,0x80000008,0,0,0,0,0x369590,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());
    // RPC_END owns 64 bytes; its reached completion helper additionally owns 32.
    const auto extra_scratch=static_cast<unsigned>(context->r[29].low)-96;
    checkMemory(rdram,extra_scratch,32);std::array<std::uint8_t,32> extra_frame;
    std::copy_n(memory_.begin()+extra_scratch,32,extra_frame.begin());
    try{deliverRpcEnd(rdram,context,0x369590,0,0x43418,0x43DB8);}
    catch(...){std::copy(extra_frame.begin(),extra_frame.end(),memory_.begin()+extra_scratch);throw;}
    std::copy(extra_frame.begin(),extra_frame.end(),memory_.begin()+extra_scratch);

    ++ncmd_read_calls;++sif_dma_submissions;context->r[2].low=0x760002+ncmd_read_calls*2; // Owned logical token.
}

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
