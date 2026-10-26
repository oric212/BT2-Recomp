// One native pass (native_passes.h): its name for BT2_NATIVE_PASSES, whether it runs by default, whether a draw may
// belong to its sequence (by the GS registers as the draw starts: paraLLEl-GS then keeps such draws in render passes
// of their own), and the function that checks a render pass and, when it recognises it, records its work
// (GSInterface::record_native_work).
#pragma once
#include "gs_interface.hpp"
#include <cstdint>
#include <span>

using NativeRenderPass=ParallelGS::GSInterface::NativeRenderPass;

struct NativePass {
    const char* name;
    bool on_by_default;
    bool (*draws)(const ParallelGS::RegisterState& registers);
    bool (*run)(const NativeRenderPass& pass,ParallelGS::GSInterface& gs,Vulkan::Device& device);
};

// A compute program of one of the passes' shaders: `buffers` storage buffers at set 0, bindings 0 on, and
// `push_bytes` of push constants (the build has no shader reflection).
Vulkan::Program* nativeComputeProgram(Vulkan::Device& device,std::span<const std::uint32_t> spirv,unsigned buffers,unsigned push_bytes);

// The passes (one file each).
bool alphaFillDraw(const ParallelGS::RegisterState& registers); // alpha_fill.cpp
bool alphaFill(const NativeRenderPass& pass,ParallelGS::GSInterface& gs,Vulkan::Device& device);
