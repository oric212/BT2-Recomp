#pragma once
#include "native_runtime.h"

inline void install_iop_vblank_provider(IopRuntime& rt) {
    rt.providers[{"vblank",5}]=[](IopRuntime& m,IopContext& c) {
        if(!m.current_thread || !m.threads.contains(m.current_thread))throw std::runtime_error("Vblank wait outside owned native thread");
        if(!m.vblank_event) {
            const auto id=m.next_event++;m.events.emplace(id,IopRuntime::Event{2,0,0});m.vblank_event=id;
        }
        IopContext wait=c;wait.set(4,m.vblank_event);wait.set(5,4);wait.set(6,1);wait.set(7,0);
        m.import("thevent",10,wait);return c.get(2); // Public WaitVblankEnd is void.
    };
}
