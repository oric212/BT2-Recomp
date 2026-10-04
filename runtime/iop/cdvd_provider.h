#pragma once
#include "native_runtime.h"

inline void install_iop_cdvd_provider(IopRuntime& rt) {
    rt.providers[{"cdvdman",50}]=[](IopRuntime& m,IopContext& c) {
        if(c.get(4)!=0xFFFFFFF5)throw std::runtime_error("Unsupported native CDVD service command");
        // SC(-11) returns CDVDMAN's interrupt event, without dereferencing param.
        // Explicit pre/post-CDVDSTM local checkpoint; no disc completion is generated.
        if(!m.sif_transport_attached || !m.rpc_initialized)throw std::runtime_error("CDVD event requested without attached startup services");
        if(!m.cdvd_interrupt_event) {
            const auto id=m.next_event++;
            m.events.emplace(id,IopRuntime::Event{2,2,0x29});m.cdvd_interrupt_event=id;
        }
        return m.cdvd_interrupt_event;
    };
}
