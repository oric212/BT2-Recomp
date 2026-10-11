// Mirror mode: every GIF transfer the CPU GS model accepts also goes to paraLLEl-GS; at each flip,
// every VRAM word the CPU model knows is compared (docs/plans/graphics/GPU_RENDERING_PLAN.md).
#pragma once
#include "gs/gpu_gs_backend.h"
#include "gs/gs_command_sink.h"
#include "host/display_frame.h"
#include <array>

class GpuGsMirror {
public:
    void attach(GsCommandSink& cpu);
    void flip() {gpu_.flush();}
    void compare(const GsCommandSink& cpu,unsigned frame);
    // The GPU image of the displayed circuits, from the privileged registers the game last wrote and
    // the video mode of its SetGsCrt request (2 NTSC, 3 PAL), which sets SMODE1 on hardware.
    DisplayFrame scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    ~GpuGsMirror(); // Prints the summary.
private:
    GpuGsBackend gpu_;
    bool attached_=false;
    unsigned frames_=0,differing_frames_=0,first_frame_=0;
    std::size_t first_word_=0;std::uint32_t first_cpu_=0,first_gpu_=0;
};
