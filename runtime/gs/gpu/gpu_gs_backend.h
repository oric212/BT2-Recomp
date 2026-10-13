// GS rendering on the GPU through paraLLEl-GS (docs/plans/graphics/GPU_RENDERING_PLAN.md).
// Built only with BT2_GPU_GS; Vulkan and Granite stay behind this interface.
#pragma once
#include <cstdint>
#include <memory>
#include <span>

struct DisplayFrame;

class GpuGsBackend {
public:
    GpuGsBackend();
    ~GpuGsBackend();
    // Creates the Vulkan device and the GS; throws a named error when no usable device exists.
    void init();
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
private:
    struct State;
    std::unique_ptr<State> state_;
};
