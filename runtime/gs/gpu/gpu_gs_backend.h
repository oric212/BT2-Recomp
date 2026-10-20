// GS rendering on the GPU through paraLLEl-GS (docs/plans/graphics/GPU_RENDERING_PLAN.md).
// Built only with BT2_GPU_GS; Vulkan and Granite stay behind this interface.
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <utility>

struct DisplayFrame;
class GsTextureNames;
class WindowPresenter;

class GpuGsBackend {
public:
    GpuGsBackend();
    ~GpuGsBackend();
    // Creates the Vulkan device and the GS; throws a named error when no usable device exists.
    // `super_sampling` (1, 2, 4, 8 or 16 samples per pixel): with 4 or more the scanout is at twice the
    // resolution, built from the samples (docs/plans/graphics/IMAGE_QUALITY_PLAN.md).
    void init(unsigned super_sampling=1);
    // GIF data in 128-bit units, as the GIF receives it on PATH3.
    void transfer(std::span<const std::uint8_t> data);
    // Completes queued work and returns GS memory (4 MB, GS address order) for comparison.
    std::span<const std::uint32_t> readVram();
    // The next quadwords of the local->host transfer the last TRXDIR started (zero past its end).
    void readFifo(std::span<std::uint8_t> out);
    // Submits queued work at a frame boundary (each flip) and advances the frame context.
    void flush();
    // Privileged register by its index in the 0x12000000 block (address offset / 16).
    void privileged(unsigned index,std::uint64_t value);
    // Scans out the displayed circuits for one field and reads the image back (empty when nothing is displayed).
    DisplayFrame scanout(unsigned field);
    // Scans out the displayed circuits for one field and keeps the image: its size, {0, 0} when nothing
    // is displayed. readback() copies the kept image to the processor; the window presenter shows it.
    std::pair<unsigned,unsigned> vsync(unsigned field);
    DisplayFrame readback();
    // Shows pictures in a window on this device (host/window_presenter.h); made at the first call.
    WindowPresenter& windowPresenter();
    // HD textures (docs/plans/graphics/HD_TEXTURES_PLAN.md): each texture the renderer caches is named by
    // `names` (kept in step with the renderer by the GS sink) and drawn with the pack's replacement when
    // it has one. The summary line says what was found, loaded and dropped.
    void enableHdTextures(const std::filesystem::path& pack,std::uint64_t budget_bytes,GsTextureNames& names);
    std::string hdTextureSummary() const;
    // GPU time by the renderer's stages (paraLLEl-GS timestamps), switched on before the first transfer;
    // the summary gives milliseconds per frame (per flush).
    void enableTimestamps();
    std::string timestampSummary() const;
private:
    struct State;
    std::unique_ptr<State> state_;
};
