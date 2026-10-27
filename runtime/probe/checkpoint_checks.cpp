#include "probe_state.h"
#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <algorithm>

// Checkpoint validation: the state each replayed startup checkpoint must reach at its boundary.
// Later checkpoints are validated per request by their receivers; these checks cover the boundary.

void checkRuntimeBoundary(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop) {
    const auto stopped=stop.stopped;
    const auto stop_selector=stop.selector;
    const auto stop_handler=stop.handler;
    const auto stop_target=stop.target;
    const auto stop_source=stop.source;
    const auto stop_resume=stop.resume;
    // The C library's stdio lock (created lazily after admission) is outside the startup service accounting.
    const auto stdio_locks=static_cast<std::size_t>(std::count_if(runtime.semaphores.begin(),runtime.semaphores.end(),
        [](const auto& sema){return sema.option==0x383988 || EeRuntime::stdioCallSema(sema.option);}));
    const auto semas=runtime.semaphores.size()-stdio_locks;
    const bool pending_mc_bind=runtime.dbc_rpc_calls==4 && semas==11 && stop_selector==0x77 && context.pc==0x32F028
        && runtime.semaphores[10].maximum==1 && runtime.semaphores[10].count==0 && (runtime.semaphores[10].option==0x3838D8 || (runtime.mc_rpc_binds==1 && runtime.semaphores[10].option==0x3838E8));
    const bool pending_mc_query=runtime.mc_rpc_calls==1 && semas==10 && ((stop_selector==0x77 && context.pc==0x32F028) || (stop_target==0x31DD60 && stop_source==0x332354))
        && runtime.read32(runtime.memory().data(),0x3A97DC)==0x3A6F80
        && runtime.read32(runtime.memory().data(),0x3A97E0)==1;
    const bool mc_local_semas=runtime.dbc_rpc_calls==4 && (semas==10 || pending_mc_bind || (runtime.disc_bind_semaphores==1 && semas==11 && (runtime.semaphores[10].option==0x3838D8 || runtime.semaphores[10].option==0x3838E8) && !runtime.semaphores[10].count))
        && runtime.semaphores[8].maximum==1 && runtime.semaphores[8].count==(pending_mc_query?0u:1u-runtime.mc_lock_waits-runtime.mc_lock_polls+runtime.mc_lock_signals)
        && runtime.semaphores[8].option==0x3824C8 && runtime.semaphores[9].maximum==1
        && runtime.semaphores[9].count==0 && runtime.semaphores[9].option==0x3824D8;
    const bool dbc_semas=runtime.dbc_rpc_binds==4 && runtime.dbc_rpc_calls>=1 && (semas==9 || mc_local_semas)
        && runtime.semaphores[7].maximum==127 && runtime.semaphores[7].option==0x381C28;
    require(stopped && !stop_handler && runtime.scmd_bind_calls==1 && runtime.cd_init_calls==1
        // After display setup, later RPCs are validated per request by their receivers.
        && (runtime.display_reset_phase==13 || (runtime.post_cd.supplied ? (semas==7 || semas==8 || dbc_semas)
            : semas==(runtime.post_sif.rpc_initialized && stop_selector==0x77?8u:7u))),"SCMD continuation state differs");
    if (!runtime.scmd_mmode_calls) {
        require(!stop_target && stop_selector == 0x40 && context.pc == 0x32EC68
                && runtime.guest_calls == 190 && runtime.returns == 185
                && context.r[29].low == runtime.initial_sp - 304 && runtime.semaphore_deletions == 3,
                "SCMD call-semaphore boundary differs");
    } else {
        // Pre-admission accounting; after main's admitted priority, helper RPC traffic is
        // validated per request by its receiver instead of by these lifetime totals.
        require(runtime.main_priority_admissions || (runtime.semaphore_deletions == 4+runtime.serviceOperations()-(runtime.mc_rpc_calls==2?1u:0u) && runtime.semaphore_waits == 5+runtime.serviceOperations()-(runtime.mc_rpc_calls==2?1u:0u)+runtime.ee_heap_lock_waits+runtime.dbc_lock_waits+runtime.mc_lock_waits && runtime.semaphore_signals == 6+runtime.serviceOperations()+(runtime.post_cd.scmd_binds?1u:0u)+runtime.ee_heap_lock_signals+runtime.dbc_lock_signals+runtime.mc_lock_signals+runtime.disc_rpc_calls+runtime.ncmd_ready_calls+runtime.ncmd_read_calls
                && (runtime.sif_dma_submissions == 5 || (runtime.iop_reset_pending && runtime.sif_dma_submissions == 6+runtime.post_sif.submissions+runtime.serviceOperations()+runtime.heap_transfers+runtime.ncmd_read_calls+runtime.cri_stream_transfers))), "SCMD media-mode cleanup differs");
        if (stop_target && !runtime.sif_stop_calls) {
            const std::array<std::array<std::uint32_t,2>,9> reached{{
                {0x338158,0x1003B8},{0x332188,0x3381D4},{0x331B08,0x332190},
                {0x32F810,0x331B10},{0x32E9D0,0x32F840},{0x32E990,0x331B20},
                {0x337F88,0x33824C},{0x33A3F0,0x337FA4},{0x32EF30,0x337FB8}}};
            require(std::any_of(reached.begin(),reached.end(),[&](const auto& entry) {
                return entry[0] == stop_target && entry[1] == stop_source && stop_resume == stop_source + 8;
            }), "Unexpected SCMD mapped-entry boundary");
        } else if (!runtime.sif_stop_calls) require(stop_selector == 0x6B && context.pc == 0x32EF38
            && runtime.guest_calls == 214 && runtime.returns == 209
            && context.r[29].low == runtime.initial_sp - 224 && GPR_U32(&context,4) == 1
            && runtime.dmac_disable_calls == 1 && runtime.dmac_removals == 1
            && (runtime.post_sif.supplied || (runtime.dmac_mask == 0x800000 && runtime.dmac_handlers.empty()))
            && runtime.read32(runtime.memory().data(),0x36AA90) == 0
            && runtime.read32(runtime.memory().data(),0x36AA8C) == 0,
            "SIF shutdown and DMA-stop boundary differs");
    }
    if (runtime.sif_stop_calls) {
        // The SIF stop and the IOP reboot (services/sif_system.cpp): SIF2 stopped; the reboot to IOPRP300
        // requested and finished (SMFLG: SIF, SIFCMD and the boot); the first boot's GS setup before it.
        require(runtime.sif_stop_calls == 1 && runtime.sif2_chcr == 0 && runtime.sif2_qwc == 0, "SIF stop state differs");
        require(runtime.iop_epoch == 1 && !runtime.prior_iop_services_valid && runtime.iop_ready_epoch == 1
                && (runtime.sif_smflag & 0x70000) == 0x70000, "IOP reboot state differs");
        require(runtime.gif_channel.starts>=2 && runtime.gs_csr_reset_writes==1 && runtime.gs_csr_reads==1
            && runtime.gs_imr_writes==1 && runtime.gs_crt_calls==1
            && (runtime.graph_startup_phase==4?runtime.graph_halfword_checkpoint:runtime.halfword_writes)==(runtime.graph_startup_phase>=3?8u:runtime.graph_startup_phase>=2?7u:4u),
            "First boot GS setup differs");
        // RPC set up again over IOPRP300's SIFCMD buffer: both INIT_CMD packets, the SET_SREG reply in the
        // EE library's table, SIF0's DMAC handler and channel enabled.
        require(runtime.post_sif.rpc_initialized && runtime.post_sif.submissions==2 && runtime.post_sif.sreg_responses==1
            && runtime.post_sif.ee_receive_address==0x3A94C0 && runtime.sif_sregs[0]==0x1E640 && runtime.sif_sregs[2]==1
            && runtime.read32(runtime.memory().data(),0x3A9740)==1 && (runtime.dmac_mask&0x200000)
            && runtime.dmac_handler_order.size()==1 && (runtime.sif0_chcr&0x100),
            "Post-reset SIF handshake differs");
        if(runtime.post_cd.binds==2 && runtime.post_cd.calls==1)
            require(runtime.read32(runtime.memory().data(),0x3A87A8)==0 && runtime.read32(runtime.memory().data(),0x3A87CC)==0x43388
                && runtime.read32(runtime.memory().data(),0x3A87BC)==0x441B8,"Fresh CD ownership/completion differs");
        // Earlier-phase snapshot; after main's admitted priority the game reuses these clients.
        if(runtime.post_cd.scmd_binds==1 && runtime.post_cd.modes==1 && !runtime.main_priority_admissions)
            require(runtime.read32(runtime.memory().data(),0x369F40)==0 && runtime.read32(runtime.memory().data(),0x369F64)==0x434F0
                && runtime.read32(runtime.memory().data(),0x369F54)==0x439A8 && runtime.read32(runtime.memory().data(),0x3695C0)==1,
                "Fresh SCMD ownership/completion differs");
    }
}

