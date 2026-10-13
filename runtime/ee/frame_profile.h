// Where a frame's host time goes, for the viewer's statistics (BT2_VIEWER_STATS): seconds spent in
// VIF1 processing (which includes the VU1 programs and the GS packets they send), in GS packet
// handling from any source, and in the GPU renderer's flush. Accumulated by scoped timers.
#pragma once
#include <chrono>

struct FrameProfile {
    double vif1=0,vu1=0,gs=0,renderer=0,flush=0; // `vif1` contains `vu1`; `vu1` contains the GS time of its packets; `gs` contains `renderer`, the GPU renderer's share.
    struct Scope {
        double& total;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
        explicit Scope(double& t):total(t) {}
        ~Scope() {total+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
    };
};
inline FrameProfile frame_profile;
