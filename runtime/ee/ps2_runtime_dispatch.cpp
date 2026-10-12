#include <cstdio>
#include "ps2_runtime.h"

// PS2Runtime guest call dispatch and kernel function binding.
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void PS2Runtime::registerKernelFunction(std::uint32_t address, RecompiledFunction function,
                                std::span<const std::uint8_t> expected_payload) {
    const bool primary = kernel_patch_end_ && address == kernel_patch_start_
        && expected_payload.size() == kernel_patch_end_ - kernel_patch_start_;
    const bool startup = address == 0x80075000 && !startup_kernel_payload_.empty()
        && expected_payload.size() == startup_kernel_payload_.size()
        && std::equal(expected_payload.begin(), expected_payload.end(), startup_kernel_payload_.begin());
    if (!function || (!primary && !startup) || functions_.contains(address))
        throw std::runtime_error("Invalid copied kernel binding");
    kernel_code_bindings_.emplace(address, std::vector<std::uint8_t>(expected_payload.begin(), expected_payload.end()));
    functions_.emplace(address, function);
}

// A field's worth of main-thread work without an idle point: the VBLANK arrives anyway, once main runs
// with interrupts enabled in the frame phase (otherwise it stays due and is retried).
void PS2Runtime::busyMainField() {
    if(!main_context_ || current_thread_id!=1 || in_interrupt_ || !gs_display_flips || !main_priority_admissions || startup_scheduler_active
        || !(intc_mask&4u) || (main_context_->cop0_status&0x10001u)!=0x10001u)return;
    ++busy_main_vblanks;deliverVblank(memory().data(),main_context_);
}

bool PS2Runtime::dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target,
                             std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char*) {
    if (kind == GuestBranchKind::Return) {
        const auto expected = call_returns_.empty() ? expected_return : call_returns_.back();
        if (target != expected) {
            char text[160];std::snprintf(text,sizeof text,"Unexpected probe return target: 0x%X, expected 0x%X, from 0x%X (thread %u, depth %zu)",target,expected,source,current_thread_id,call_returns_.size());
            throw std::runtime_error(text);
        }
        context->pc = target;
        ++returns;
        return true;
    }
    last_branch_source_=source;last_branch_target_=target;
    if(target==0x2EF218 && source==0x1007F0 && current_thread_id==1 && main_priority_admissions && !startup_scheduler_active) {
        // Main's asynchronous load poll found the request incomplete. Logical time model: one
        // VBLANK_START per idle call, so woken helpers advance the request in observed order.
        // No frame timing is claimed; the original needed 120 VBlanks for this request.
        if(++load_poll_idles>1000)throw UnsupportedGuestBranch(target,source,resume);
        deliverVblank(rdram,context);
    }
    if(target==0x2EF218 && source==0x100D20 && current_thread_id==1 && main_priority_admissions && !startup_scheduler_active
        && display_reset_phase==13) {
        // Main's synchronous file load (0x100CB8) polls the same file service; same logical time model.
        if(++file_load_idles>20000)throw UnsupportedGuestBranch(target,source,resume);
        deliverVblank(rdram,context);
    }
    if(target==0x2EC990 && source==0x100E38 && current_thread_id==1 && frameGifContext() && timer1_stops) {
        // The overlay's indexed-asset loop polls the CRI file status with no idle call. The same
        // logical time model: each status poll is one idle point receiving one VBLANK_START.
        if(++asset_poll_idles>20000)throw UnsupportedGuestBranch(target,source,resume);
        deliverVblank(rdram,context);
    }
    if(current_thread_id==1 && !in_interrupt_) {
        main_context_=context;
        if((kind==GuestBranchKind::DirectCall || kind==GuestBranchKind::IndirectCall) && ++main_calls_since_field_>main_calls_per_field)busyMainField();
    }
    const auto found = functions_.find(target);
    if(target==0x32FDA0 && source==0x315210 && sound_rpc_calls==1) {
        if(!sound_thread_start_supplied || sound_thread_checkpoint || current_thread_id!=1 || main_thread_priority!=2
            || threads.size()!=2 || threads[1].id!=3 || context->r[4].low!=3 || context->r[5].low
            || context->cop0_status!=0x10000)throw UnsupportedGuestBranch(target,source,resume);
        // Independent caller capture supplies only the reached IE/EIE bits.
        // Other CP0 fields remain outside this bootstrap compatibility scope.
        context->cop0_status|=0x10001;sound_thread_checkpoint=true;
    }
    if (found == functions_.end()) throw UnsupportedGuestBranch(target, source, resume);
    verifyKernelBinding(target);
    if (dispatch_depth_ >= 64) throw std::runtime_error("Bootstrap call depth exceeded");
    struct DepthGuard {
        unsigned& depth;
        explicit DepthGuard(unsigned& value) : depth(value) { ++depth; }
        ~DepthGuard() { --depth; }
    } depth_guard(dispatch_depth_);
    if (phase_stop && target == phase_stop) {phase_stop = 0; throw UnsupportedGuestBranch(target, source, resume);}
    if (kind == GuestBranchKind::TailJump) {
        entered_functions.insert(target);
        context->pc = target;
        found->second(rdram, context, this);
        completed_functions.insert(target);
        return true;
    }
    if (!resume || (resume & 3)) throw std::runtime_error("Invalid guest return address");
    entered_functions.insert(target);
    call_returns_.push_back(resume);
    ++guest_calls;
    context->pc = target;
    try {
        found->second(rdram, context, this);
        if (context->pc != resume) throw std::runtime_error("Guest callee did not return to caller: target "+std::to_string(target)+" source "+std::to_string(source)+" resume "+std::to_string(resume)+" pc "+std::to_string(context->pc));
    } catch (...) { if (stop_returns.empty()) stop_returns = call_returns_; call_returns_.pop_back(); throw; }
    call_returns_.pop_back();
    completed_functions.insert(target);
    if (phase_stop && resume == phase_stop) {phase_stop = 0; throw UnsupportedGuestBranch(target, source, resume);}
    return true;
}

void PS2Runtime::verifyKernelBinding(std::uint32_t address) {
    if (const auto overlay = overlay_code_bindings_.find(address); overlay != overlay_code_bindings_.end()) {
        if (memory_.size() < address + overlay->second.size()
                || !std::equal(overlay->second.begin(), overlay->second.end(), memory_.begin() + address))
            throw std::runtime_error("Loaded overlay code differs from its generated binding");
        ++overlay_dispatch_validations;
        return;
    }
    const auto found = kernel_code_bindings_.find(address);
    if (found == kernel_code_bindings_.end()) return;
    for (std::size_t offset = 0; offset < found->second.size(); offset += 4) {
        std::uint32_t expected;
        std::memcpy(&expected, found->second.data() + offset, 4);
        const auto word = kernel_patch_words_.find(address - 0x80000000 + static_cast<std::uint32_t>(offset));
        if (word == kernel_patch_words_.end() || word->second != expected)
            throw std::runtime_error("Copied kernel payload differs from compiled binding");
    }
    ++kernel_dispatch_validations;
}
