// Where a frame's host time goes, for the viewer's statistics (BT2_VIEWER_STATS): seconds spent in
// VIF1 processing (which includes the VU1 programs and the GS packets they send), in GS packet
// handling from any source, and in the GPU renderer's flush. Accumulated by scoped timers. One per
// thread: the game's thread reads its own; the GS thread's work (gs/gpu/gs_render_thread.h) is added
// to the game thread's as `renderer` where the game's thread waits for it.
#pragma once
#include <chrono>

struct FrameProfile {
    double vif1=0,vu1=0,vu1_gs=0,gs=0,flush=0; // `vif1` contains `vu1`; `vu1` contains the GS time of its packets; `vu1_gs` is the part of `gs` that VU1's packets take.
    double renderer=0,gs_wait=0; // The GS threads' working time, and the game's thread waiting for them (not part of `gs`).
    bool enabled=false; // Off, the scopes cost a test; on, two clock reads each.
    struct Scope {
        double* total;std::chrono::steady_clock::time_point start;
        explicit Scope(double& t);
        ~Scope() {if(total)*total+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
    };
};
inline thread_local FrameProfile frame_profile;
inline FrameProfile::Scope::Scope(double& t):total(frame_profile.enabled?&t:nullptr) {if(total)start=std::chrono::steady_clock::now();}
