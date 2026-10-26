// The GPU renderer as the runtime uses it: every GIF transfer the GS command sink accepts goes to
// paraLLEl-GS, which draws every picture (docs/plans/graphics/GPU_RENDERING_PLAN.md).
//
// The main renderer (attach) takes the data on a GS thread of its own (gs/gpu/gs_render_thread.h): the
// game's thread only checks and copies it. The texture names follow it there, in step with the renderer
// (a sink of the GS thread's own runs the accepted transfers again with the names; gs_command_sink.h).
// A second renderer on the device (the in-between pictures) has a GS thread of its own in the same way,
// at the same time as the main one. Every other call waits for the renderer's GS thread first, so each
// renderer is used by one thread at a time (the second renderer's other calls come from one thread of the
// caller's, with the same command pools); the window presenter's calls come after a vsync, submit or
// readback, which wait. A frame change of the device (the main renderer's flip, a readback, a picture
// shown in the window) waits until no command buffer is open, so the second renderer's GS thread
// submits its work and stands still meanwhile (GsRenderThread::whilePaused): drawing a whole frame at
// once, it would hold the device's frame for many milliseconds. (Two renderers drawing at once corrupted
// the heap within seconds until Granite's oversized staging blocks were retired under its lock:
// granite_retired_blocks.patch.)
#pragma once
#include "gs/gpu/gpu_gs_backend.h"
#include "gs/gpu/gs_render_thread.h"
#include "gs/cpu/gs_command_sink.h"
#include "ee/frame_profile.h"
#include "host/display_frame.h"
#include "host/window_presenter.h"
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class GpuGsMirror {
public:
    void attach(GsCommandSink& cpu,unsigned super_sampling=1);
    // A second renderer on `primary`'s device fed by `cpu` (the in-between pictures of 60 fps battles). Its
    // GS thread has thread index 1, and so must the one thread that makes its other calls
    // (GpuGsBackend::registerRenderThread).
    void attachShared(GsCommandSink& cpu,GpuGsMirror& primary,unsigned super_sampling=1);
    // The texture names follow the renderer's input (BT2_TEXTURE_NAMES, HD textures). Before the first transfer.
    void nameTextures(GsTextureNames& names);
    void shareHdTextures(const GpuGsMirror& primary,GsTextureNames& names,bool late_palettes) {drain();gpu_.shareHdTextures(primary.gpu_,names,late_palettes);named_.texture_names=&names;}
    // The window shows `source`'s pictures (null: this renderer's).
    // Which picture the window presenter shows; only the presenter reads it, so the GS thread is not waited for.
    void showPictureOf(const GpuGsMirror* source) {gpu_.showPictureOf(source?&source->gpu_:nullptr);}
    void setQuadResolution(bool on) {gpu_.setQuadResolution(on);} // gpu_gs_backend.h
    void holdPicture(unsigned slot) {const auto hold=use();gpu_.holdPicture(slot);} // gpu_gs_backend.h
    // The window shows `source`'s held picture `slot` (null source: this renderer's; slot -1: the last scanout).
    void showHeld(const GpuGsMirror* source,int slot) {gpu_.showHeld(source?&source->gpu_:nullptr,slot);}
    void flip() {const auto hold=use();FrameProfile::Scope timed(frame_profile.flush);gpu_.awaitFrameChange();whileFollowersPaused([this]{gpu_.flush();});}
    void keepState(unsigned slot) {const auto hold=use();gpu_.keepState(slot);}
    // `primary`'s kept state must be complete (made at its last flip).
    void takeState(const GpuGsMirror& primary,unsigned slot) {const auto hold=use();gpu_.takeState(primary.gpu_,slot);}
    void submit() {const auto hold=use();gpu_.submit();} // The work so far, without ending the frame (gpu_gs_backend.h).
    void readFifo(std::span<std::uint8_t> out) {const auto hold=use();gpu_.readFifo(out);} // Local->host data, GPU-only mode.
    void enableTimestamps() {const auto hold=use();gpu_.enableTimestamps();}
    void enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names,bool playstation_buttons=true);
    // The GPU image of the displayed circuits, from the privileged registers the game last wrote and
    // the video mode of its SetGsCrt request (2 NTSC, 3 PAL), which sets SMODE1 on hardware.
    DisplayFrame scanout(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    // The same scanout kept on the GPU (its size; {0, 0} when nothing is displayed), for the window
    // presenter; readback() copies it to the processor.
    std::pair<unsigned,unsigned> vsync(const std::array<std::uint64_t,15>& privileged,unsigned crt_mode,unsigned field);
    DisplayFrame readback() {const auto hold=use();gpu_.awaitFrameChange();DisplayFrame frame;whileFollowersPaused([&]{frame=gpu_.readback();});return frame;}
    // Its pictures are shown on a thread of its own (gpu_window_presenter.h), each while this renderer's GS
    // thread and the second renderers' stand still with their work submitted (not drained: the main GS thread
    // may be well into the next frame).
    WindowPresenter& windowPresenter();
    // Waits for the renderer's GS thread, except on it, and rethrows its failure; its time and the wait go
    // to the frame profile.
    void drain();
    ~GpuGsMirror(); // Prints the summary.
private:
    // Runs `fn` (a frame change of the device) while every renderer attached to this one stands still with
    // its work submitted.
    void whileFollowersPaused(const std::function<void()>& fn,std::size_t from=0);
    void whilePaused(const std::function<void()>& fn); // The same, with this renderer's GS thread too.
    GpuGsMirror* primary_=nullptr;std::vector<GpuGsMirror*> followers_; // attachShared's.
    // The renderer's calls on the caller's thread hold this after draining (use), and so does a pause's
    // submit on the GS thread, which may come between the two.
    std::mutex use_;
    std::unique_lock<std::mutex> use() {drain();return std::unique_lock<std::mutex>(use_);}
    GpuGsBackend gpu_;
    void startThread(GsCommandSink& cpu,unsigned thread_index);
    GsCommandSink named_; // The GS thread's: transfers again with the texture names (none without names).
    std::unique_ptr<GsRenderThread> thread_; // After the renderer: stopped first.
};
