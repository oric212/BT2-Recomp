#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. Disc requests: CDVD file search, N-command readiness/reads and S-commands.

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
    // The library creates the call's wait semaphore per blocking call: the lowest free kernel ID,
    // which is 10 during startup and higher once later objects exist. Use the client's own ID.
    const auto sema=read32(rdram,0x369598);
    if(!native_iop || iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d
        || (repeat ? !(startup_scheduler_active && current_thread_id>=4 && current_thread_id<=7) || !ncmd_read_calls
                     || rid!=read32(rdram,0x369594) || read32(rdram,0x3A97C0)!=0x40 || sema<10
                   : d!=initial_sp-768 || rid!=45 || sema!=10)
        || context->r[5].low!=1 || semaphores.size()<=sema || semaphores[sema].count
        || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
    const std::array<unsigned,4> desc={0x203A97C0,0x1E640,64,0x44};
    const std::array<unsigned,14> packet={0x40,0,0x8000000A,0,5,0x203A97C0,rid,0x369590,14,0,0x368440,4,1,0x43418};
    for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<14;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
    if(read32(rdram,0x3695A4)!=0x43DB8 || read32(rdram,0x3695B4)!=0x43418)throw UnsupportedSyscall(0x77,context->pc);
    auto& rt=native_iop->runtime;unsigned result;
    try{result=native_iop->callDiscReady(iop_epoch);}
    catch(const std::exception& e){native_iop_error=e.what();native_iop_pc=rt.executing_pc;native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());throw UnsupportedSyscall(0x77,context->pc);}
    write32(rdram,0x368440,result);
    const std::array<unsigned,16> reply={0x400,0x368440,0x80000008,0,0,0,0,0x369590,0x8000000A,0,0,0,0,0,0,0};
    for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x369590,sema,0x43418,0x43DB8);++ncmd_ready_calls;++sif_dma_submissions;
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
