// 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): a picture between each two fight
// frames, drawn from the game's own commands with the camera and the bones halfway. The game keeps
// stepping 30 times a second exactly as before; this only draws and never touches guest state.
//
// Each frame's GS input is recorded in order: VIF1 transfers (with the GS data their run produced) and
// all other GIF data. A second renderer on the main one's GPU device follows the game one frame behind:
// at each flip it draws the in-between picture of the last two frames (the newer frame's input with its
// VIF1 lists blended against the older frame's, run on a copy of the VIF1/VU1 state at the newer frame's
// start), scans it out, and then replays the newer frame's real input, so its GS state stays the main
// renderer's. The list sent during flip F is on screen at flip F + 2, so when F is complete the picture
// between F - 1 and F can be shown a field before F's own: no delay is added.
#pragma once
#include "gs/cpu/gs_command_sink.h"
#include "gs/gpu/gpu_gs_mirror.h"
#include "gs/textures/gs_texture_names.h"
#include "vu/vif1.h"
#include <array>
#include <memory>
#include <span>
#include <vector>

class InBetweenFrames {
public:
    // `vif1` is the VIF1 state now (the first frame's start). With HD textures the second renderer names
    // textures itself and draws with the primary's pack.
    InBetweenFrames(GpuGsMirror& primary,const Vif1& vif1,unsigned super_sampling,bool hd_textures);

    // Recording, from the runtime's hooks: a VIF1 transfer before it runs, and each GS input (`from_vif1`:
    // produced by the VIF1 transfer that runs now).
    void vif1Transfer(std::span<const std::uint32_t> words);
    void gsInput(std::span<const std::uint8_t> data,bool from_vif1);

    // At a display flip: the frame recorded since the last flip is complete; `fight` says it was a fight
    // frame (it waited for the fight's 30 fps step). `vif1` is the VIF1 state as the next frame starts.
    // Draws the in-between picture of the last two frames when both were fight frames whose lists pair,
    // scanning it out with `privileged` (the display registers of the flip that will show the newer
    // frame), then replays the newer frame. Returns whether an in-between picture is ready.
    bool flip(bool fight,const Vif1& vif1,const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    std::pair<unsigned,unsigned> pictureSize() const {return picture_;}
    GpuGsMirror& renderer() {return mirror_;}
    // For checks: when set, the frames are only replayed (no in-between pictures) and each replay is
    // scanned out, so the second renderer's picture can be compared with the main one's.
    bool replay_only=false;
    unsigned drawn=0,unpaired=0,not_fights=0;

private:
    struct Event {bool vif1=false;std::vector<std::uint32_t> words;std::vector<std::vector<std::uint8_t>> output;};
    struct Frame {std::vector<Event> events;std::unique_ptr<Vif1> start;bool fight=false;};
    Frame recording_,previous_;
    GsCommandSink sink_;
    GsTextureNames names_;
    GpuGsMirror mirror_;
    std::pair<unsigned,unsigned> picture_{0,0};
    void send(std::span<const std::uint8_t> data);
    bool drawInBetween(const Frame& older,const Frame& newer);
};
