#pragma once
#include "iop/core/native_runtime.h"

// High-level IOP kernel and library services: each installer registers the import providers
// (library, ordinal) that IopRuntime::import resolves before native module exports.

// IOP kernel libraries, one installer per functional group.
void install_iop_system_providers(IopRuntime& rt);     // stdio, sysmem, secrman, sysclib, modload
void install_iop_interrupt_providers(IopRuntime& rt);  // intrman
void install_iop_dma_providers(IopRuntime& rt);        // dmacman (SIO2 card DMA)
void install_iop_sync_providers(IopRuntime& rt);       // thsemap, thevent
void install_iop_time_providers(IopRuntime& rt);       // thbase delays, alarms and clock
void install_iop_thread_providers(IopRuntime& rt);     // thbase threads
inline void install_iop_kernel_providers(IopRuntime& rt) {
    install_iop_system_providers(rt);install_iop_sync_providers(rt);install_iop_thread_providers(rt);
    install_iop_interrupt_providers(rt);install_iop_dma_providers(rt);install_iop_time_providers(rt);
}
// SleepThread/WakeupThread/iWakeupThread and TIMRMAN, installed only while a later sound command runs.
void install_iop_sound_scope_sleep(IopRuntime& rt);

// Module-facing libraries.
void install_iop_library_provider(IopRuntime& rt);  // loadcore export registration
void install_iop_ioman_provider(IopRuntime& rt);    // ioman device drivers
void install_iop_sif_rpc_provider(IopRuntime& rt);  // sifman/sifcmd RPC servers
void install_iop_vblank_provider(IopRuntime& rt);   // vblank waits
void install_iop_cdvd_provider(IopRuntime& rt);     // cdvdman interrupt event
