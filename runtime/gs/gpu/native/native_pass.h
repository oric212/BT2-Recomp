// One native pass (native_passes.h): its name for BT2_NATIVE_PASSES, whether it runs by default, whether a draw may
// belong to its sequence (by the GS registers as the draw starts: paraLLEl-GS then keeps such draws in render passes
// of their own), and the function that checks a render pass and, when it recognises it, records its work
// (GSInterface::record_native_work).
#pragma once
#include "gs_interface.hpp"
#include <cstdint>
#include <span>

using NativeRenderPass=ParallelGS::GSInterface::NativeRenderPass;

// What a pass's work may use besides GS memory: the renderer's device and a scratch buffer of its own (one per
// renderer, reused: the renderer's work is ordered on its queue).
struct NativeContext {
    ParallelGS::GSInterface& gs;
    Vulkan::Device& device;
    Vulkan::BufferHandle scratch_buffer;
    const Vulkan::Buffer& scratch(VkDeviceSize bytes);
};

struct NativePass {
    const char* name;
    bool on_by_default;
    bool (*draws)(const ParallelGS::RegisterState& registers);
    bool (*run)(const NativeRenderPass& pass,NativeContext& context);
};

// A compute program of one of the passes' shaders: `buffers` storage buffers at set 0, bindings 0 on, then
// `uniforms` uniform buffers, then `textures` combined image samplers, `push_bytes` of push constants and
// `spec_constants` specialization constants from id 0 (the build has no shader reflection).
Vulkan::Program* nativeComputeProgram(Vulkan::Device& device,std::span<const std::uint32_t> spirv,unsigned buffers,unsigned push_bytes,unsigned uniforms=0,unsigned textures=0,unsigned spec_constants=0);

// The passes (one file each).
bool alphaFillDraw(const ParallelGS::RegisterState& registers); // alpha_fill.cpp
bool alphaFill(const NativeRenderPass& pass,NativeContext& context);
bool inkEdgesDraw(const ParallelGS::RegisterState& registers); // ink_edges.cpp
bool inkEdges(const NativeRenderPass& pass,NativeContext& context);
bool glowDraw(const ParallelGS::RegisterState& registers); // glow.cpp
bool glow(const NativeRenderPass& pass,NativeContext& context);
