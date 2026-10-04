#include "ps2_runtime.h"
#include "module_service.h"
void PS2Runtime::receiveNativeModule(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<std::uint32_t>(context->r[4].low);
        if(context->r[29].low!=d || d!=initial_sp-528 || context->r[5].low!=2 || semaphores.size()!=8
            || semaphores[7].count || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8 || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> descriptor={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<8;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        const auto id=read32(rdram,0x3ACC04);
        const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,id,0x3ACC00,0,512,0x3ACA00,8,1,0x22568};
        for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(!id || read32(rdram,0x3ACC00)!=0x203A97C0 || read32(rdram,0x3ACC08)!=7 || read32(rdram,0x3ACA00)!=0)
            throw UnsupportedSyscall(0x77,context->pc);
        std::string path;for(unsigned n=0;n<252;++n) {const auto ch=memory_.at(0x3ACA08+n);if(!ch)break;if(ch<32 || ch>126)throw UnsupportedSyscall(0x77,context->pc);path+=static_cast<char>(ch);}
        native_module_request=path;
        if(path.empty() || path.size()==252)throw UnsupportedSyscall(0x77,context->pc);
        if(!native_iop) {native_iop=std::make_shared<NativeIopService>(iop_epoch,bt2_compiled_iop_modules());native_iop->attachEeTransport(iop_epoch); }
        if(!native_iop->supports(path))throw UnsupportedSyscall(0x77,context->pc);
        NativeIopService::Resident resident{};
        try {resident=native_iop->load(iop_epoch,path);}
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
