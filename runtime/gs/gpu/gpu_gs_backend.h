// GS rendering on the GPU through paraLLEl-GS (docs/plans/graphics/GPU_RENDERING_PLAN.md).
// Built only with BT2_GPU_GS; Vulkan and Granite stay behind this interface.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>
#include <string>
#include <utility>

struct DisplayFrame;
class GsTextureNames;
class WindowPresenter;

class HdTexturePack;

class GpuGsBackend {
public:
    GpuGsBackend();
    ~GpuGsBackend();
    // Creates the Vulkan device and the GS; throws a named error when no usable device exists.
    // `super_sampling` (1, 2, 4, 8 or 16 samples per pixel): with 4 or more the scanout is at twice the
    // resolution, built from the samples (docs/plans/graphics/IMAGE_QUALITY_PLAN.md).
    void init(unsigned super_sampling=1);
    // A second renderer on `primary`'s device (the in-between pictures of 60 fps battles), with its own
    // GS state; its pictures can be shown in `primary`'s window (showPictureOf).
    void initShared(GpuGsBackend& primary,unsigned super_sampling=1);
    // The window shows `source`'s last scanout (null or this: its own).
    void showPictureOf(const GpuGsBackend* source);
    // With 16 samples, scanouts at four times the resolution (one sample per pixel) instead of twice
    // (tools/graphics/gpu/parallel_gs_quad_scanout.patch); from the next scanout, any thread.
    void setQuadResolution(bool on);
    // Pictures kept for showing later (the delayed pictures of 60 fps battles): the last scanout into one of
    // held_slots slots, and the window shows `source`'s held picture in `slot` (showHeld with slot -1: its
    // last scanout again, as showPictureOf).
    static constexpr unsigned held_slots=4;
    void holdPicture(unsigned slot);
    void showHeld(const GpuGsBackend* source,int slot);
    // GIF data in 128-bit units, as the GIF receives it on PATH3.
    void transfer(std::span<const std::uint8_t> data);
    // Completes queued work and returns GS memory (4 MB, GS address order) for comparison.
    std::span<const std::uint32_t> readVram();
    // The renderer's state on the GPU (GS memory with its super-samples, and the CLUT) kept in one of
    // kept_slots slots after a flush (keepState; three, as the in-between pictures may run a frame behind), and a second renderer on the same device set to a kept state
    // (takeState; its cached textures are dropped): the in-between pictures of 60 fps battles start from
    // the main renderer's state. Copies on the GPU in submission order; neither waits for the GPU
    // (tools/graphics/gpu/parallel_gs_vram_backup.patch).
    static constexpr unsigned kept_slots=3;
    void keepState(unsigned slot);
    void takeState(const GpuGsBackend& primary,unsigned slot);
    // The next quadwords of the local->host transfer the last TRXDIR started (zero past its end).
    void readFifo(std::span<std::uint8_t> out);
    // Submits queued work at a frame boundary (each flip) and advances the frame context.
    void flush();
    // Before a frame change of the device (flush, readback, a picture shown in the window): waits for the GPU
    // work that Granite's frame change waits for while it holds the device's lock (the frame context it
    // reuses: about 7 changes back), so that wait stops no other thread; notes this change's work.
    void awaitFrameChange();
    // Submits the work taken so far without ending the frame. Between flips the renderer holds a command
    // buffer, and the device's next frame context (a readback, the window presenter) waits for it.
    void submit();
    // Called once on a GS thread (gs/gpu/gs_render_thread.h): the main renderer's (index 0) records with
    // the game thread's command pools, which the two use one at a time; a second renderer's has index 1.
    static void registerRenderThread(unsigned index);
    // Privileged register by its index in the 0x12000000 block (address offset / 16).
    void privileged(unsigned index,std::uint64_t value);
    // Scans out the displayed circuits for one field and reads the image back (empty when nothing is displayed).
    DisplayFrame scanout(unsigned field);
    // Scans out the displayed circuits for one field and keeps the image: its size, {0, 0} when nothing
    // is displayed. readback() copies the kept image to the processor; the window presenter shows it.
    std::pair<unsigned,unsigned> vsync(unsigned field);
    DisplayFrame readback();
    // Shows pictures in a window on this device (host/window_presenter.h), on a thread of its own that runs
    // each of its frames through `around_frame` (gpu_window_presenter.h); made at the first call.
    WindowPresenter& windowPresenter(std::function<void(const std::function<void()>&)> around_frame);
    void closeWindowPresenter(); // Stops its thread (before what around_frame uses goes).
    // HD textures (docs/plans/graphics/HD_TEXTURES_PLAN.md): each texture the renderer caches is named by
    // `names` (kept in step with the renderer by the GS sink) and drawn with the pack's replacement when
    // it has one. The summary line says what was found, loaded and dropped. `budget_bytes` 0: half of the video
    // memory the card has free now (at least 256 MB, at most 8 GB). `playstation_buttons`: the pack's
    // PlayStation button art (else its Xbox art).
    void enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names,bool playstation_buttons=true);
    std::string hdTextureSummary() const;
    HdTexturePack* hdTexturePack(); // The pack (null without one), for tests.
    // The second renderer draws with `primary`'s pack (one copy in video memory), named by its own `names`;
    // `late_palettes`: it draws each frame after the primary's flip, so the palettes the primary drew reach
    // it one flip late (else at once, for a renderer that draws a frame while the primary does).
    void shareHdTextures(const GpuGsBackend& primary,GsTextureNames& names,bool late_palettes);
    // GPU time by the renderer's stages (paraLLEl-GS timestamps), switched on before the first transfer;
    // the summary gives milliseconds per frame (per flush).
    void enableTimestamps();
    std::string timestampSummary() const;
private:
    void copyDrawnPalettes(); // After a flush: the drawn palettes back to the texture names.
    void replaceTextures(GsTextureNames& names);
    void dumpTexture(GsTextureNames& names,std::uint64_t tex0,const std::string& name); // BT2_TEXTURE_DUMP.
    struct State;
    std::unique_ptr<State> state_;
};
