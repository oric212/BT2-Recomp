// Native passes inside paraLLEl-GS (docs/plans/graphics/HYBRID_RENDERING_PLAN.md). Each render pass the renderer is
// about to draw is offered to the enabled passes in turn; one that recognises it, by what the GS was given (buffers,
// formats, primitives, state), records GPU work of its own that leaves GS memory as the renderer's drawing would, and
// the renderer skips it (tools/graphics/gpu/parallel_gs_native_passes.patch). Anything not recognised in full is
// drawn as before. BT2_NATIVE_PASSES=<name>[,<name>...] (or "none") chooses them for runs; without it, the passes on by
// default run.
#pragma once
#include <memory>
#include <string>

namespace ParallelGS {class GSInterface;}
namespace Vulkan {class Device;}

class NativePasses {
public:
    // Hooks the enabled passes into `gs`, which draws on `device`; nothing when none is enabled.
    NativePasses(ParallelGS::GSInterface& gs,Vulkan::Device& device);
    ~NativePasses();
    std::string summary() const; // How often each pass ran; empty when none did.
private:
    struct State;
    std::unique_ptr<State> state_;
};
