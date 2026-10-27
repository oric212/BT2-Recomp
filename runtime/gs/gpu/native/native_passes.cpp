#include "gs/gpu/native/native_passes.h"
#include "gs/gpu/native/native_pass.h"
#include "device.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {
// Every pass, in the order a render pass is offered to them.
constexpr std::array passes{
    // The flat alpha byte written over a rectangle of a frame (the groundwork's check of the hook; the ink chain
    // starts with one): off by default.
    NativePass{"alpha-fill",false,alphaFillDraw,alphaFill},
    // The ink outline's edges per sample: the game's at 1x, its thickness with smooth edges with super-sampling
    // (docs/plans/graphics/HYBRID_RENDERING_PLAN.md step 2); on, the look the user chose on 2026-10-27.
    NativePass{"ink-edges",true,inkEdgesDraw,inkEdges},
    // A super attack's glow and the blended layers between its quads, shaded once per game pixel with super-sampling
    // (docs/plans/graphics/HYBRID_RENDERING_PLAN.md step 3); on (the user's decision of 2026-10-27).
    NativePass{"glow",true,glowDraw,glow},
};

// BT2_NATIVE_PASSES: the passes named (comma-separated), "none", or without it those on by default; names written
// "-name" take a pass away from those on by default (the player setting "HD outline" off: "-ink-edges").
std::array<bool,passes.size()> enabledPasses() {
    std::array<bool,passes.size()> on{};
    const char* setting=std::getenv("BT2_NATIVE_PASSES");
    const std::string_view text(setting?setting:"");
    if(!setting || text.starts_with('-'))for(std::size_t n=0;n<passes.size();++n)on[n]=passes[n].on_by_default;
    if(!setting || text=="none")return on;
    for(std::size_t at=0;at<=text.size();) {
        const auto end=std::min(text.find(',',at),text.size());
        auto name=text.substr(at,end-at);
        const bool remove=name.starts_with('-');
        if(remove)name.remove_prefix(1);
        std::size_t n=0;
        while(n<passes.size() && name!=passes[n].name)++n;
        if(n==passes.size())throw std::runtime_error("BT2_NATIVE_PASSES: no native pass named '"+std::string(name)+"'");
        on[n]=!remove;at=end+1;
    }
    return on;
}
}

Vulkan::Program* nativeComputeProgram(Vulkan::Device& device,std::span<const std::uint32_t> spirv,unsigned buffers,unsigned push_bytes,unsigned uniforms,unsigned textures,unsigned spec_constants) {
    Vulkan::ResourceLayout layout;
    for(unsigned b=0;b<buffers;++b) {layout.sets[0].storage_buffer_mask|=1u<<b;layout.sets[0].meta[b].array_size=1;}
    for(unsigned b=buffers;b<buffers+uniforms;++b) {layout.sets[0].uniform_buffer_mask|=1u<<b;layout.sets[0].meta[b].array_size=1;}
    for(unsigned b=buffers+uniforms;b<buffers+uniforms+textures;++b) {
        layout.sets[0].sampled_image_mask|=1u<<b;layout.sets[0].fp_mask|=1u<<b;layout.sets[0].meta[b].array_size=1;
    }
    layout.push_constant_size=push_bytes;
    layout.spec_constant_mask=(1u<<spec_constants)-1u;
    return device.request_program(spirv.data(),spirv.size_bytes(),&layout);
}

const Vulkan::Buffer& NativeContext::scratch(VkDeviceSize bytes) {
    if(!scratch_buffer || scratch_buffer->get_create_info().size<bytes) {
        Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::Device;
        info.size=bytes;info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        scratch_buffer=device.create_buffer(info);
    }
    return *scratch_buffer;
}

struct NativePasses::State {
    explicit State(ParallelGS::GSInterface& gs,Vulkan::Device& device):context{gs,device,{}} {}
    NativeContext context;
    ParallelGS::GSInterface* hooked=nullptr;
    std::array<bool,passes.size()> on{};
    std::array<std::atomic<std::uint64_t>,passes.size()> runs{};
};

NativePasses::NativePasses(ParallelGS::GSInterface& gs,Vulkan::Device& device):state_(std::make_unique<State>(gs,device)) {
    state_->on=enabledPasses();
    bool any=false;for(const bool on:state_->on)any=any || on;
    if(!any)return;
    gs.native_render_pass=[state=state_.get()](const NativeRenderPass& pass) {
        for(std::size_t n=0;n<passes.size();++n)
            if(state->on[n] && passes[n].run(pass,state->context)) {++state->runs[n];return true;}
        return false;
    };
    // A draw's kind: 1 + the first enabled pass it may belong to, 0 for none.
    gs.native_draw_kind=[state=state_.get()](const ParallelGS::RegisterState& registers)->std::uint32_t {
        for(std::size_t n=0;n<passes.size();++n)if(state->on[n] && passes[n].draws(registers))return std::uint32_t(n+1);
        return 0;
    };
    state_->hooked=&gs;
}

NativePasses::~NativePasses() {
    if(!state_->hooked)return;
    state_->hooked->native_render_pass=nullptr;state_->hooked->native_draw_kind=nullptr;
}

std::string NativePasses::summary() const {
    std::string text;
    for(std::size_t n=0;n<passes.size();++n) {
        if(!state_->runs[n])continue;
        text+=(text.empty()?"Native passes: ":", ")+std::string(passes[n].name)+" "+std::to_string(state_->runs[n].load());
    }
    return text;
}
