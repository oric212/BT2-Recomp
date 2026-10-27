// Showing pictures in the viewer window on the GPU (gs/gpu/gpu_gs_backend.h) instead of with GDI:
// the renderer's picture goes to the window without being read back to the processor, and a stretch
// is filtered on the GPU (docs/plans/ee/PERFORMANCE_PLAN.md, presenting without readback).
#pragma once
#include "host/display_frame.h"
#include <chrono>

class WindowPresenter {
public:
    struct Rect {int x=0,y=0,width=0,height=0;};
    virtual ~WindowPresenter()=default;
    // Shows a picture in the window's client area (client_width x client_height), black around it:
    // `frame` (a picture drawn by the processor, such as a menu), or with nullptr the renderer's last
    // scanout. `smooth`: filtered scaling, else each pixel a block. `status` (may be empty) is drawn
    // one pixel for one at `status_at`'s corner.
    virtual void show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
                      const DisplayFrame& status,Rect status_at)=0;
    // The next picture shown is presented no earlier than `at` (a picture scheduled a field ahead, while the
    // game goes on). Presenters that show at once ignore it.
    virtual void presentAt(std::chrono::steady_clock::time_point) {}
};
