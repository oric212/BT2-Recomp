#include "ps2_runtime.h"
#include "module_service.h"
void PS2Runtime::receiveNativeModule(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<std::uint32_t>(context->r[4].low);
        const bool buffer=read32(rdram,0x3A97E0)==6;
        if(context->r[29].low!=d || d!=initial_sp-(buffer?640:528) || context->r[5].low!=2 || semaphores.size()!=8
            || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8 || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> descriptor={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        const auto id=read32(rdram,0x3ACC04);
        const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,id,0x3ACC00,buffer?6u:0u,512,0x3ACA00,8,1,0x22568};
        for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(!id || read32(rdram,0x3ACC00)!=0x203A97C0 || read32(rdram,0x3ACC08)!=7 || read32(rdram,0x3ACA00)!=(buffer?0xCE500u:0u))
            throw UnsupportedSyscall(0x77,context->pc);
        std::string path;
        if(buffer) {
            if(!native_iop || native_module_loads!=13 || heap_dma_polls!=1 || read32(rdram,0x3ACA04))throw UnsupportedSyscall(0x77,context->pc);
            // With arg_len zero, the caller's unused argument/path residue is not consumed.
            path="embedded:ck01";
        }else for(unsigned n=0;n<252;++n) {const auto ch=memory_.at(0x3ACA08+n);if(!ch)break;if(ch<32 || ch>126)throw UnsupportedSyscall(0x77,context->pc);path+=static_cast<char>(ch);}
        native_module_request=path;
        if(path.empty() || path.size()==252)throw UnsupportedSyscall(0x77,context->pc);
        if(!native_iop) {native_iop=std::make_shared<NativeIopService>(iop_epoch,bt2_compiled_iop_modules());native_iop->attachEeTransport(iop_epoch); }
        if(!buffer && !native_iop->supports(path))throw UnsupportedSyscall(0x77,context->pc);
        NativeIopService::Resident resident{};
        try {resident=buffer?native_iop->loadBootBuffer(iop_epoch,0xCE500):native_iop->load(iop_epoch,path);}
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
        deliverRpcEnd(rdram,context,0x3ACC00,7,0x22568,0x225B0);
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
    if(!heap_allocation_supplied || heap_calls || heap_binds!=1 || native_module_loads!=13 || !native_iop || iop_epoch!=1
        || prior_iop_services_valid || context->r[29].low!=d || d!=initial_sp-528 || context->r[5].low!=2
        || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> desc={0x3AC8C0,0x48480,4,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x440,0x48480,0x8000000A,0,5,0x203A97C0,23,0x3AC840,1,4,0x3AC880,4,1,0x48438};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC8C0)!=0x360 || read32(rdram,0x3AC840)!=0x203A97C0 || read32(rdram,0x3AC844)!=23
        || read32(rdram,0x3AC848)!=7 || read32(rdram,0x3AC854)!=0x48480 || read32(rdram,0x3AC864)!=0x48438)
        throw UnsupportedSyscall(0x77,context->pc);
    const auto allocated=native_iop->allocateBootHeap(iop_epoch,read32(rdram,0x3AC8C0));
    write32(rdram,0x3AC880,allocated);
    const std::array<std::uint32_t,16> reply={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC840,7,0x48438,0x48480);
    ++heap_calls;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls;
    native_iop_steps=native_iop->runtime.steps;native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());native_iop_pc=native_iop->runtime.executing_pc;
}

void PS2Runtime::receiveHeapCopy(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(!heap_copy_supplied || heap_transfers || heap_calls!=1 || heap_binds!=1 || !native_iop || iop_epoch!=1
        || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-144 || context->r[5].low!=1)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,4> descriptor={0x369F90,0xCE500,0x360,0};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
    if(native_iop->boot_heap_allocations!=1)throw UnsupportedSyscall(0x77,context->pc);
    checkMemory(rdram,descriptor[0],descriptor[2]);native_iop->runtime.check(descriptor[1],descriptor[2]);
    std::copy_n(memory_.data()+descriptor[0],descriptor[2],native_iop->runtime.ram.data()+descriptor[1]);
    ++heap_transfers;++sif_dma_submissions;context->r[2].low=0x50001; // Owned logical transfer token.
}

void PS2Runtime::receiveHeapFree(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(!native_iop || native_module_loads!=14 || heap_calls!=1 || heap_frees || iop_epoch!=1 || prior_iop_services_valid
        || context->r[29].low!=d || d!=initial_sp-544 || context->r[5].low!=2 || semaphores.size()!=8
        || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8)
        throw UnsupportedSyscall(0x77,context->pc);
    const std::array<std::uint32_t,8> desc={0x3AC8C0,0x48480,4,0,0x203A97C0,0x1E640,64,0x44};
    const std::array<std::uint32_t,14> packet={0x440,0x48480,0x8000000A,0,5,0x203A97C0,25,0x3AC840,2,4,0x3AC880,4,1,0x48438};
    for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3AC8C0)!=0xCE500 || read32(rdram,0x3AC840)!=0x203A97C0 || read32(rdram,0x3AC844)!=25
        || read32(rdram,0x3AC848)!=7 || read32(rdram,0x3AC854)!=0x48480 || read32(rdram,0x3AC864)!=0x48438)
        throw UnsupportedSyscall(0x77,context->pc);
    write32(rdram,0x3AC880,native_iop->freeBootHeap(iop_epoch,0xCE500));
    const std::array<std::uint32_t,16> reply={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x3AC840,7,0x48438,0x48480);
    ++heap_frees;++sif_dma_submissions;context->r[2].low=0x40000+heap_binds+heap_calls+heap_frees;
    native_iop_steps=native_iop->runtime.steps;native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());native_iop_pc=native_iop->runtime.executing_pc;
}
