#include "ee/ps2_runtime.h"
#include "iop/services/module_service.h"
#include <algorithm>
#include <iostream>

// EE side of the native IOP bridge. Sound requests: the sound-driver and SOUNDS RPCs, forwarded to the native sound services.

void PS2Runtime::receiveSoundRpc(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(display_reset_phase==13 && sounds_rpc_calls>=1 && native_iop && read32(rdram,0x3A97DC)==0x6351C0) {
        // Main's later SOUNDS commands: 256-byte request from 0x3B0E00, 64-byte reply, live client RPC ID.
        auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(0x02000004);
        if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto server=found->second;const auto rid=read32(rdram,0x6351C4),sema=read32(rdram,0x6351C8),command=read32(rdram,0x3A97E0);
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d || context->r[5].low!=2
            || current_thread_id!=1 || startup_scheduler_active || !rid || sema>=semaphores.size() || semaphores[sema].count
            || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> desc={0x3B0E00,server.buffer,256,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x10040,server.buffer,0x8000000A,0,5,0x203A97C0,rid,0x6351C0,command,256,0x635180,64,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x6351C0)!=0x203A97C0 || read32(rdram,0x6351E4)!=server.descriptor || read32(rdram,0x6351D4)!=server.buffer)
            throw UnsupportedSyscall(0x77,context->pc);
        // Command 0x20 starts the SOUNDS worker, which allocates an IOP hardware timer (timrman) for its
        // periodic tick; IOP timers are not modelled yet. Stop before the IOP is entered.
        if(!sounds_hardware_timers_supported)throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callSoundsCommand(iop_epoch,command,std::span<const std::uint8_t>(memory_.data()+0x3B0E00,256));}
        catch(const std::exception& e) {
            native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
            throw UnsupportedSyscall(0x77,context->pc);
        }
        std::copy(response.begin(),response.end(),memory_.begin()+0x635180);
        const std::array<std::uint32_t,16> reply={0x4000,0x635180,0x80000008,0,0,0,0,0x6351C0,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x6351C0,sema,server.descriptor,server.buffer);
        ++sounds_rpc_calls;++sif_dma_submissions;context->r[2].low=0x60004+sounds_rpc_calls;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
    if(display_reset_phase==13 && sound_rpc_calls>=1 && native_iop && read32(rdram,0x3A97DC)==0x3A5C80) {
        // Main's later sound commands: 64-byte request, 16-byte reply, live client RPC ID.
        auto& rt=native_iop->runtime;const auto found=rt.rpc_servers.find(0x80000701);
        if(found==rt.rpc_servers.end())throw UnsupportedSyscall(0x77,context->pc);
        const auto server=found->second;const auto rid=read32(rdram,0x3A5C84),sema=read32(rdram,0x3A5C88),command=read32(rdram,0x3A97E0);
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d || context->r[5].low!=2
            || current_thread_id!=1 || startup_scheduler_active || !rid || sema>=semaphores.size() || semaphores[sema].count
            || semaphores[sema].maximum!=1 || semaphores[sema].option!=0x3838E8)throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> desc={0x3A5C40,server.buffer,64,0,0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,14> packet={0x4040,server.buffer,0x8000000A,0,5,0x203A97C0,rid,0x3A5C80,command,64,0x3A5C40,16,1,server.descriptor};
        for(unsigned n=0;n<desc.size();++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<packet.size();++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A5C80)!=0x203A97C0 || read32(rdram,0x3A5CA4)!=server.descriptor || read32(rdram,0x3A5C94)!=server.buffer
            || read32(rdram,0x3A5CA0)!=0x3A5C40)throw UnsupportedSyscall(0x77,context->pc);
        // The first later command (0x80D0) starts a LIBSD voice transfer (SPU2 DMA4 + IRQ 36), which
        // the owned SPU2 device does not model yet: stop before the IOP is entered (EE request validated).
        if(!sound_voice_transfers_supported)throw UnsupportedSyscall(0x77,context->pc);
        std::vector<std::uint8_t> response;
        try {response=native_iop->callSoundCommand(iop_epoch,command,std::span<const std::uint8_t>(memory_.data()+0x3A5C40,64));}
        catch(const std::exception& e) {
            native_iop_error=e.what();native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;
            throw UnsupportedSyscall(0x77,context->pc);
        }
        std::copy(response.begin(),response.end(),memory_.begin()+0x3A5C40);
        const std::array<std::uint32_t,16> reply={0x1000,0x3A5C40,0x80000008,0,0,0,0,0x3A5C80,0x8000000A,0,0,0,0,0,0,0};
        for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x3A5C80,sema,server.descriptor,server.buffer);
        ++sound_rpc_calls;++sif_dma_submissions;context->r[2].low=0x60002+sound_rpc_calls;
        native_iop_steps=rt.steps;native_iop_targets=static_cast<unsigned>(rt.entries.size());native_iop_pc=rt.executing_pc;return;
    }
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
