#pragma once

// Authored bootstrap adapter for generated probes. Broader PS2 APIs fail explicitly.
#include <string>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <span>
#include <set>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <iostream>
#include <immintrin.h>
#include "intc_checkpoint.h"
#include "retained_ee_stack.h"
#include "gif_dma_staging.h"
#include "logical_timer2.h"
#include "logical_timer1.h"
#include "post_reset_sif.h"
#include "vif_startup.h"
#include "texture_gif_startup.h"
#include <memory>
struct EeExecutionOwner;
#ifdef BT2_NATIVE_IOP
#include <memory>
class NativeIopService;
#endif

struct alignas(16) ProbeGpr {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    operator __m128i() const {
        __m128i value;
        std::memcpy(&value, this, sizeof(value));
        return value;
    }
};

struct R5900Context {
    std::array<ProbeGpr, 32> r{};
    std::uint32_t pc = 0;
    std::uint64_t hi = 0, lo = 0, hi1 = 0, lo1 = 0;
    std::uint32_t sa = 0, fcr31 = 0, cop0_status = 0, branch_pc = 0;
    std::uint32_t cop0_epc = 0, cop0_errorepc = 0;
    // Generated ABI storage only; VIF/VU reset execution remains gated at first MMIO.
    std::uint32_t vu0_fbrst = 0;
    bool ll_bit = false;
    bool in_delay_slot = false;
    std::array<float, 32> f{};
    float f_acc = 0;
};

struct UnsupportedSyscall : std::runtime_error {
    UnsupportedSyscall(std::uint32_t selector, std::uint32_t pc, std::uint32_t handler = 0)
        : std::runtime_error("Unsupported bootstrap syscall"), selector(selector), resume_pc(pc), handler(handler) {}
    std::uint32_t selector, resume_pc, handler;
};

struct UnsupportedGuestBranch : std::runtime_error {
    UnsupportedGuestBranch(std::uint32_t target, std::uint32_t source, std::uint32_t resume)
        : std::runtime_error("Unsupported bootstrap guest dispatch"), target(target), source_pc(source), resume_pc(resume) {}
    std::uint32_t target, source_pc, resume_pc;
};

struct UnsupportedGuestMemory : std::runtime_error {
    UnsupportedGuestMemory(std::uint32_t address, std::size_t size)
        : std::runtime_error("Unsupported or out-of-bounds guest memory access"), address(address), size(size) {}
    std::uint32_t address;
    std::size_t size;
};

struct UnsupportedVuStatus : std::runtime_error {
    UnsupportedVuStatus():std::runtime_error("Unsupported VU0 VPU_STAT read") {}
};

class PS2Runtime {
public:
    // BC2T/BC2F condition (VU activity). In the frame model no VU microprogram is ever started.
    bool cop2Condition(R5900Context*) {
        if(frameGifContext())return false;
        throw UnsupportedVuStatus();
    }
    std::uint32_t readVuStatus(R5900Context* context) {
        if(clear_repeat_gif && displayResetStep(context,6,0x31062C,624))return clear_repeat_gif->readVuIdle();
        if(frameGifContext() && (context->pc==0x31062C || context->pc==0x31064C))return 0; // VU1 never started in the frame model.
        if(gif_dma.syncSupplied() && context->pc==0x31062C && context->r[29].low==initial_sp-624)
            return gif_dma.readVuIdle();
        throw UnsupportedVuStatus();
    }
    using RecompiledFunction = void (*)(std::uint8_t*, R5900Context*, PS2Runtime*);
    // Diagnostic handoff: retain the parent while running the ready sound child.
    // The priority request remains pending; the first wait is a checked boundary.
    void probeReadySoundThread(std::uint8_t*, R5900Context*);
    bool ee_sound_dispatch_probe = false, ee_sound_probe_attempted = false;
    // Diagnostic phase stop for capture comparisons: the first entry to this address, or the first
    // return to it, stops as a branch boundary. Zero disables it; it never changes guest state.
    std::uint32_t phase_stop = 0;
    std::vector<std::uint32_t> stop_returns; // Guest return addresses live when a stop first unwound.
    R5900Context ee_sound_probe_frontier{}, ee_sound_probe_parent{};
    unsigned ee_sound_probe_waits = 0;
    RetainedEeStack* active_ee_stack = nullptr;
    bool ee_queued_sound_rpc=false, sound_thread_waiting=false;
    struct QueuedSoundsBind { bool pending=false;unsigned epoch=0;std::array<unsigned,16> reply{}; } queued_sounds_bind;
    unsigned queued_sound_submissions=0,queued_sound_completions=0,sound_dispatch_bind_id=0;
    std::uint32_t sound_dispatch_wait_ra=0,sound_dispatch_wait_sp=0;
    unsigned startup_worker_dispatches=0,startup_worker_status_queries=0,startup_worker_waits=0;
    R5900Context startup_dispatch_parent{};
    void dispatchReadyStartupWorkers(std::uint8_t*,R5900Context*);
    // Scheduler scope opened by main's admitted priority request. Dispatched workers use
    // kernel thread semantics; any transition requiring preemption fails closed.
    bool startup_scheduler_active=false;
    unsigned scheduled_priority_changes=0,scheduled_resumes=0,scheduled_suspends=0,main_priority_admissions=0;
    std::uint32_t main_priority_previous=0;
    unsigned cri_stream_transfers=0,load_poll_idles=0,asset_poll_idles=0,dbc_port_calls=0,dbc_main_calls=0,scmd_calls=0;
    bool handleScheduledThreadSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    void requireNoPreemption(std::uint32_t,const R5900Context*) const;
    bool handleAdmittedIntcSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    unsigned vblank_handler_registrations=0,vblank_enable_calls=0,vblank_disable_calls=0;
    bool in_interrupt_=false;
    unsigned vblank_deliveries=0,worker_wakes=0,worker_resumes=0,worker_sleep_returns=0;
    bool handleInterruptSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    void deliverVblank(std::uint8_t*,R5900Context*);
    void advanceLogicalField();
    void runIntcHandlers(std::uint8_t*,R5900Context*,std::uint32_t,const char*);
    void runReadyThreads(std::uint8_t*,R5900Context*);
    void queueSoundsBind(R5900Context*,const std::array<unsigned,16>&);
    void finishQueuedSoundsBind(std::uint8_t*,R5900Context*);


    enum class GuestBranchKind { Return, DirectCall, IndirectCall, TailJump, IndirectJump=TailJump, DirectJump=TailJump };
    static constexpr std::size_t ram_size = 32 * 1024 * 1024;

    void clearLLBit(R5900Context* context) { context->ll_bit = false; }

    void registerFunction(std::uint32_t address, RecompiledFunction function) {
        if (address >= 0x80000000 || !function || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate guest registration");
    }

    // A function generated from a runtime-loaded overlay: dispatchable only while main RAM holds the
    // exact bytes it was generated from (checked on every dispatch; EE_OVERLAY_PLAN.md).
    void registerOverlayFunction(std::uint32_t address, RecompiledFunction function, std::span<const std::uint8_t> expected) {
        if (address >= ram_size || !function || expected.empty() || expected.size() % 4 || expected.size() > ram_size - address
                || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate overlay registration");
        overlay_code_bindings_.emplace(address, std::vector<std::uint8_t>(expected.begin(), expected.end()));
    }

    void registerKernelFunction(std::uint32_t address, RecompiledFunction function,
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

    void callFunction(std::uint32_t address, R5900Context& context) {
        const auto found = functions_.find(address);
        if (found == functions_.end()) throw std::runtime_error("Unregistered guest target");
        verifyKernelBinding(address);
        context.pc = address;
        checkpoints_ = 0;
        entered_functions.insert(address);
        found->second(memory_.empty() ? nullptr : memory_.data(), &context, this);
        completed_functions.insert(address);
    }

    // Isolated logical RPC_END callback validation. The caller supplies checked
    // capture state; this does not emulate interrupt entry or schedule delivery.
    void validateRpcEndCallback(R5900Context& context) {
        if (rpc_end_callback_active_ || context.r[4].low != 0x3A94C0
                || context.r[5].low != 0x3AAFC0 || context.r[28].low != 0x38BDF0)
            throw std::runtime_error("Invalid isolated RPC_END callback context");
        rpc_end_callback_active_ = true;
        try { callFunction(0x3322E8, context); }
        catch (...) { rpc_end_callback_active_ = false; throw; }
        rpc_end_callback_active_ = false;
    }

    void expectStartupZeroRegion(std::uint32_t start, std::uint32_t end) {
        if (startup_zero_end_ || thread_initialized || end <= start || end > memory_.size())
            throw std::runtime_error("Invalid startup zero checkpoint");
        startup_zero_start_ = start;
        startup_zero_end_ = end;
    }

    void handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code) {
        const auto selector = static_cast<std::uint32_t>(context->r[3].low);
        if (code == 0 && selector < syscall_handlers.size() && syscall_handlers[selector]) {
            const auto handler = syscall_handlers[selector];
            if (!functions_.contains(handler)) throw UnsupportedSyscall(selector, context->pc, handler);
            // Narrow host callback model: preserve wrapper RA, validate native callback return.
            // Does not emulate BIOS exception entry, privilege transitions or ERET.
            const auto saved_ra = context->r[31];
            const auto resume = context->pc;
            constexpr std::uint32_t callback_return = 0xFFFFFFF8;
            context->r[31].low = callback_return;
            ++custom_syscall_calls;
            try {
                dispatchGuestBranch(rdram, context, handler, resume, callback_return,
                                    GuestBranchKind::DirectCall, "registered syscall callback");
            } catch (...) { context->r[31] = saved_ra; throw; }
            context->r[31] = saved_ra;
            context->pc = resume;
            return;
        }
        if(code==0 && selector==0x73 && frameGifContext() && timer1_stops && context->pc==0x32EFD8) {
            const auto flag=static_cast<std::uint32_t>(context->r[4].low),csr=static_cast<std::uint32_t>(context->r[5].low);
            if(vsync_flag || flag!=context->r[29].low || csr!=flag+8 || (flag&15))throw UnsupportedSyscall(selector,context->pc);
            checkMemory(rdram,flag,4);checkMemory(rdram,csr,8);
            vsync_flag=flag;vsync_csr=csr;return; // Void BIOS API preserves V0 in the original capture.
        }
        if(code==0 && in_interrupt_) {
            if(handleInterruptSyscall(rdram,selector,context))return;
            throw UnsupportedSyscall(selector,context->pc); // Interrupt context fails closed.
        }
        if(code==0 && ((startup_scheduler_active && active_ee_stack && current_thread_id>=4 && current_thread_id<=7)
                || (main_priority_admissions && !startup_scheduler_active && current_thread_id==1))
            && handleScheduledThreadSyscall(rdram,selector,context)) return;
        if(code==0 && main_priority_admissions && !startup_scheduler_active && current_thread_id==1
            && handleAdmittedIntcSyscall(rdram,selector,context)) return;
#ifdef BT2_NATIVE_IOP
        if(code==0 && selector==0x77 && startup_scheduler_active && active_ee_stack && current_thread_id>=4 && current_thread_id<=7
            && isCriStreamSubmission(rdram,context)) {receiveCriStream(rdram,context);return;}
        if(code==0 && selector==0x77 && startup_scheduler_active && active_ee_stack && current_thread_id>=4 && current_thread_id<=7
            && read32(rdram,0x3A97DC)==0x369F40) {receiveScmdCall(rdram,context);return;}
        if(code==0 && selector==0x77 && disc_rpc_binds==1 && (disc_rpc_calls==0 || (disc_rpc_calls>=1 && disc_rpc_calls<6 && ncmd_read_calls==1)) && !local_disc_path.empty() && read32(rdram,0x3A97DC)==0x3A8780) {receiveDiscSearch(rdram,context);return;}
#endif
#ifdef BT2_NATIVE_IOP
        if(code==0 && selector==0x77 && cri_rpc_binds==1 && (cri_rpc_calls>=1 && cri_rpc_calls<=41 && cri_rpc_calls!=35) && read32(rdram,0x3A97DC)==0x355C70) {receiveCriControl(rdram,context);return;}
        if(code==0 && selector==0x77 && cri_rpc_binds==1 && (!cri_rpc_calls || (cri_rpc_calls==35 && heap_calls==5)) && read32(rdram,0x3A97DC)==0x355C70) {receiveCriCreate(rdram,context);return;}
        if(code==0 && selector==0x77 && disc_rpc_calls==6 && ncmd_read_calls==1 && !cri_rpc_binds && read32(rdram,0x3A97DC)==0x355C70) {receiveCriBind(rdram,context);return;}
        if(code==0 && selector==0x77 && ((ncmd_ready_calls==1 && !ncmd_read_calls) || (ncmd_read_calls && startup_scheduler_active))
            && read32(rdram,0x3A97DC)==0x369590 && read32(rdram,0x3A97E0)==1){receiveNcmdRead(rdram,context);return;}
        if(code==0 && selector==0x77 && ncmd_rpc_binds==1 && (!ncmd_ready_calls || (ncmd_read_calls && startup_scheduler_active))
            && read32(rdram,0x3A97DC)==0x369590 && read32(rdram,0x3A97E0)==14){receiveNcmdReady(rdram,context);return;}
#endif
        if(code==0 && selector==0x77 && ncmd_bind_supplied && disc_rpc_calls==1 && !ncmd_rpc_binds && read32(rdram,0x3A97DC)==0x369590) {receiveNcmdBind(rdram,context);return;}
        if(code==0 && selector==0x77 && disc_bind_supplied && mc_rpc_calls==2 && disc_bind_semaphores==1
            && read32(rdram,0x3A97DC)==0x3A8780 && !disc_rpc_binds) {receiveDiscBind(rdram,context);return;}
        if(code==0 && selector==0x77 && post_cd.supplied && post_sif.rpc_initialized && iop_epoch==1
            && (post_cd.binds<2 || !post_cd.calls)) {receiveFreshCd(rdram,context);return;}
        if(code==0 && selector==0x77 && post_loadfile.supplied && post_cd.modes==1 && post_sif.rpc_initialized && iop_epoch==1
            && (!post_loadfile.binds || !post_loadfile.versions)) {receiveFreshLoadfile(rdram,context);return;}
#ifdef BT2_NATIVE_IOP
        if(code==0 && selector==0x77 && dbc_rpc_calls==4 && mc_rpc_calls<=1 && read32(rdram,0x3A97DC)==0x3A6F80) {receiveMcRpc(rdram,context);return;}
        if(code==0 && selector==0x77 && dbc_rpc_calls==4 && (read32(rdram,0x3A97DC)==0x3A0460 || read32(rdram,0x3A97DC)==0x3A0438)) {receiveDbcPort(rdram,context);return;}
        if(code==0 && selector==0x77 && dbc_rpc_calls==4 && read32(rdram,0x3A97DC)==0x3A03C0 && frameGifContext()) {receiveDbcMain(rdram,context);return;}
        if(code==0 && selector==0x77 && dbc_rpc_binds==4 && dbc_rpc_calls>=1 && dbc_rpc_calls<4 && read32(rdram,0x3A97DC)==0x3A03C0) {receiveDbcBind(rdram,context);return;}
        if(code==0 && selector==0x77 && sounds_rpc_calls==1 && read32(rdram,0x3A97DC)==(dbc_rpc_binds<2?(dbc_rpc_calls?0x3A0410u:0x3A03C0u):(dbc_rpc_binds==2?0x3A0438u:0x3A0460u)) && dbc_rpc_binds<4) {receiveDbcBind(rdram,context);return;}
        if(code==0 && selector==0x77 && display_reset_phase==13 && sounds_rpc_calls>=1 && read32(rdram,0x3A97DC)==0x6351C0
            && current_thread_id==1 && main_priority_admissions && !startup_scheduler_active) {receiveSoundRpc(rdram,context);return;}
        if(code==0 && selector==0x77 && display_reset_phase==13 && sound_rpc_calls>=1 && read32(rdram,0x3A97DC)==0x3A5C80
            && current_thread_id==1 && main_priority_admissions && !startup_scheduler_active) {receiveSoundRpc(rdram,context);return;}
        if(code==0 && selector==0x77 && heap_calls==2 && native_module_loads==14 && (read32(rdram,0x3A97DC)==0x3A5C80 || (sound_thread_starts==1 && read32(rdram,0x3A97DC)==0x6351C0))) {receiveSoundRpc(rdram,context);return;}
#ifdef BT2_NATIVE_IOP
        if(code==0 && selector==0x77 && display_reset_phase==13 && current_thread_id==1 && main_priority_admissions
            && !startup_scheduler_active && !in_interrupt_ && context->r[31].low==0x28BA48) {receiveIopUpload(rdram,context);return;}
        if(code==0 && selector==0x77 && display_reset_phase==13 && current_thread_id==1 && main_priority_admissions
            && !startup_scheduler_active && !in_interrupt_ && read32(rdram,0x3A97DC)==0x3AC740) {
            if(!fileio_binds)receiveFileioBind(rdram,context);else receiveFileioCall(rdram,context);
            return;
        }
        if(code==0 && selector==0x76 && iop_uploads && context->r[4].low==0x5A0000+iop_uploads && context->pc==0x32F008
            && context->r[31].low==0x28BA58 && current_thread_id==1 && !startup_scheduler_active) {
            ++iop_upload_polls;context->r[2].low=UINT64_MAX;return; // Owned synchronous copy is complete; no DMA timing.
        }
#endif
        if(code==0 && selector==0x77 && heap_copy_supplied && ((heap_calls==1 && !heap_transfers
            && read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+4)==0xCE500) || (embeddedReload() && heap_calls==7 && heap_transfers==1
            && read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+4)==0x196100))) {receiveHeapCopy(rdram,context);return;}
        if(code==0 && selector==0x77 && ((native_module_loads==14 && heap_calls==1 && !heap_frees)
                || (embeddedReload() && native_module_loads==15 && heap_calls==7 && heap_frees==1))
            && read32(rdram,0x3A97DC)==0x3AC840 && read32(rdram,0x3A97E0)==2) {receiveHeapFree(rdram,context);return;}
        if(code==0 && selector==0x76 && heap_copy_supplied && heap_transfers==heap_dma_polls+1 && (!heap_dma_polls || (embeddedReload() && heap_dma_polls==1))) {
            if(iop_epoch!=1 || context->pc!=0x32F008 || context->r[4].low!=0x50001+heap_dma_polls || context->r[29].low!=initial_sp-144)
                throw UnsupportedSyscall(selector,context->pc);
            ++heap_dma_polls;context->r[2].low=UINT64_MAX;return; // Owned synchronous copy is complete; no physical DMA timing.
        }
        if(code==0 && selector==0x77 && heap_allocation_supplied && heap_binds==1 && (!heap_calls || (heap_calls==1 && heap_frees==1 && texture_gif.phase==8) || (heap_calls==2 && cri_rpc_binds==1 && disc_rpc_calls==6 && ncmd_read_calls==1) || (heap_calls==3 && cri_rpc_calls==3 && disc_rpc_calls==6 && ncmd_read_calls==1) || (heap_calls==4 && cri_rpc_calls==35) || (heap_calls==5 && cri_rpc_calls==36) || (heap_calls==6 && embeddedReload()) || ((heap_calls==7 || heap_calls==8) && display_reset_phase==13 && sound_rpc_calls>=3))
            && read32(rdram,0x3A97DC)==0x3AC840 && read32(rdram,0x3A97C8)==0x8000000A) {receiveHeapAllocation(rdram,context);return;}
        if(code==0 && selector==0x77 && heap_bind_supplied && native_module_loads==13 && !heap_binds
            && read32(rdram,0x3A97E0)==0x80000003) {receiveHeapBind(rdram,context);return;}
        if(code==0 && selector==0x77 && post_loadfile.versions==1 && post_cd.modes==1 && post_sif.rpc_initialized && iop_epoch==1) {
            receiveNativeModule(rdram,context);return;
        }
#endif
        if(code==0 && selector==0x77 && post_cd.scmd_supplied && post_cd.calls==1
            && (!post_cd.scmd_binds || !post_cd.modes)) {receiveFreshScmd(rdram,context);return;}
        if(code==0 && post_sif.supplied && gif_dma.completions==2 && iop_epoch==1) {
            if(selector==0x78 && post_sif.phase==2) {
                post_sif.chain();return; // SDK declares void, retain v0.
            }
            if(selector==0x7A) {context->r[2].low=post_sif.get(static_cast<std::uint32_t>(context->r[4].low));return;}
            if(selector==0x79) {post_sif.set(static_cast<std::uint32_t>(context->r[4].low),static_cast<std::uint32_t>(context->r[5].low));context->r[2].low=context->r[5].low;return;}
            if(selector==0x77 && (post_sif.phase==10 || post_sif.phase==12)) {
                auto d=static_cast<std::uint32_t>(context->r[4].low);checkMemory(rdram,d,16);
                auto source=read32(rdram,d),count=read32(rdram,d+8);
                if(context->r[5].low!=1 || read32(rdram,d+4)!=0x1E640 || read32(rdram,d+12)!=0x44 || source!=(post_sif.phase==10?0x3A9580u:0x3A9800u))
                    throw UnsupportedSyscall(selector,context->pc);
                checkMemory(rdram,source,count);
                auto result=post_sif.submit(memory().subspan(source,count));
                sif_ee_receive_address=post_sif.ee_receive_address;++sif_dma_submissions;
                if(post_sif.sreg_responses) {
                    // Captured SET_SREG index 0/value 1 response, logical delivery.
                    constexpr std::array<std::uint32_t,6> reply={0,0,0x80000001,0,0,1};
                    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
                    write32(rdram,0x3A9740,1);
                }
                context->r[2].low=result;return;
            }
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 4 && scmd_mmode_supplied_) {
            receiveScmdMmode(rdram,context); return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 3 && scmd_bind_supplied_) {
            receiveScmdBind(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 2 && cd_init_supplied_) {
            receiveFirstCdInit(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 1 && !sif_bind_iop_.empty()) {
            receiveFirstRpcBind(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && reset_submission_supplied_ && sif_dma_submissions == 5) {
            if (iop_reset_pending || reset_smflag_writes != 1 || sif_reset_subaddr_reads != 1
                    || context->r[4].low != initial_sp-224 || context->r[29].low != context->r[4].low
                    || context->r[5].low != 1) throw UnsupportedSyscall(selector,context->pc);
            const std::array<std::uint32_t,4> descriptor{0x3ACC40,0x19600,104,0x44};
            const std::array<std::uint32_t,6> header{104,0,0x80000003,0,36,0};
            for(unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])throw UnsupportedSyscall(selector,context->pc);
            for(unsigned i=0;i<6;++i) if(read32(rdram,0x3ACC40+i*4)!=header[i])throw UnsupportedSyscall(selector,context->pc);
            constexpr char path[]="rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
            if(!std::equal(std::begin(path),std::end(path),memory_.begin()+0x3ACC58))throw UnsupportedSyscall(selector,context->pc);
            // Submission acknowledgement only. Retain the request and invalidate
            // old service lifetime; no receive, image execution or ready flag invented.
            std::copy_n(memory_.data()+0x3ACC40,104,reset_request_.data());
            iop_reset_pending=true; prior_iop_services_valid=false; ++iop_epoch;
#ifdef BT2_NATIVE_IOP
            native_iop.reset();native_module_loads=0;
#endif
            ++sif_dma_submissions;context->r[2].low=0xE01;return;
        }
        if (code == 0 && selector == 0x77) {
            if (sif_iop_.empty() || sif_dma_submissions || sif_command_reads != 1 || data_cache_lines != 1
                    || context->r[5].low != 1 || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
            if (descriptor & 15) throw UnsupportedGuestMemory(descriptor, 16);
            checkMemory(rdram, descriptor, 16);
            const auto source = read32(rdram, descriptor), destination = read32(rdram, descriptor + 4);
            if (source != 0x3A9580 || destination != 0x19600 || read32(rdram, descriptor + 8) != 20
                    || read32(rdram, descriptor + 12) != 0x44)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, source, 20);
            // Observed CHANGE_SADDR command only; unknown commands fail before mutation.
            if (read32(rdram, source) != 20 || read32(rdram, source + 4) != 0
                    || read32(rdram, source + 8) != 0x80000000 || read32(rdram, source + 12) != 0
                    || read32(rdram, source + 16) != 0x3A94C0)
                throw UnsupportedSyscall(selector, context->pc);
            std::copy_n(memory_.data() + source, 20, sif_iop_.data() + destination);
            // Model only the validated logical receive effects, synchronously.
            // The service consumes psize and stores the new EE receive address.
            sif_iop_[destination] = 0;
            sif_ee_receive_address = 0x3A94C0;
            std::memcpy(sif_iop_.data() + 0x196C8, &sif_ee_receive_address, 4);
            ++sif_dma_submissions;
            context->r[2].low = sif_dma_result_;
            return;
        }
        if (code == 0 && selector == 0x79 && iop_reset_pending) {
            const auto reg=static_cast<std::uint32_t>(context->r[4].low);
            const auto value=static_cast<std::uint32_t>(context->r[5].low);
            const std::array<std::uint32_t,4> regs{4,4,0x80000002,0x80000000};
            const std::array<std::uint32_t,4> values{0x10000,0x20000,0,0};
            if(!reset_submission_supplied_ || reset_clear_writes>=4 || reg!=regs[reset_clear_writes] || value!=values[reset_clear_writes]
                    || context->r[29].low!=initial_sp-224)throw UnsupportedSyscall(selector,context->pc);
            if(reset_clear_writes<2) { sif_smflag&=~value;context->r[2].low=sif_smflag; }
            else { if(reset_clear_writes==2) reset_rpcinit=0;else reset_subaddr=0;context->r[2].low=0; }
            ++reset_clear_writes;return;
        }
        if (code == 0 && selector == 0x79 && sif_stop_calls) {
            if (!reset_smflag_supplied_ || reset_smflag_writes || sif_reset_subaddr_reads != 1
                    || context->r[4].low != 4 || context->r[5].low != 0x40000
                    || context->r[29].low != initial_sp - 224 || sif_smflag != 0x30000)
                throw UnsupportedSyscall(selector, context->pc);
            // Captured kernel behavior: EE SMFLAG write clears selected bits,
            // returning the readback value (different from the pinned Play! return).
            sif_smflag &= ~0x40000u; ++reset_smflag_writes;
            context->r[2].low = sif_smflag;
            return;
        }
        if (code == 0 && selector == 0x6B) {
            if (!sif_stop_supplied_ || sif_stop_calls || scmd_mmode_calls != 1 || sif_dma_submissions != 5
                    || dmac_removals != 1 || dmac_mask != 0x800000 || !dmac_handlers.empty()
                    || context->r[4].low != 1 || context->r[29].low != initial_sp - 224)
                throw UnsupportedSyscall(selector, context->pc);
            // Local kernel handler writes zero to SIF2 CHCR/QWC and returns QWC.
            // All supported logical submissions have finished; active DMA is unsupported.
            sif2_chcr = 0; sif2_qwc = 0; ++sif_stop_calls; context->r[2].low = 0;
            return;
        }
        if(code==0 && selector==0x71 && gs_imr_supplied_) {
            if(context->r[4].low==0xFF00 && context->r[31].low==0x30FE6C
                && (displayResetStep(context,2,0x32EFA8,688) || displayResetStep(context,10,0x32EFA8,128))) {
                gs_imr=0xFF00;++display_reset_phase;context->r[2].low=0xFF00;return;
            }
            if(graph_startup_phase==2 && context->pc==0x32EFA8 && context->r[4].low==0xFF00
                && context->r[29].low==initial_sp-112 && context->r[31].low==0x30FE6C) {
                gs_imr=0xFF00;graph_startup_phase=3;context->r[2].low=0xFF00;return;
            }
            if(gs_csr_reads!=1 || gs_imr_writes || context->pc!=0x32EFA8 || context->r[4].low!=0xFF00
                    || context->r[29].low!=initial_sp-688 || context->r[31].low!=0x30FE6C)
                throw UnsupportedSyscall(selector,context->pc);
            gs_imr=0xFF00;++gs_imr_writes;context->r[2].low=0xFF00;return;
        }
        if(code==0 && selector==2 && gs_crt_supplied_) {
            if(context->r[4].low==1 && context->r[5].low==2 && ((context->r[6].low==1 && context->r[31].low==0x100274
                    && displayResetStep(context,4,0x32E868,608)) || (!context->r[6].low && context->r[31].low==0x2710A0
                    && displayResetStep(context,11,0x32E868,48)))) {
                gs_crt_request={1,2,static_cast<std::uint32_t>(context->r[6].low)};++display_reset_phase;context->r[2].low=0;return;
            }
            if(graph_startup_phase==3 && context->pc==0x32E868 && context->r[4].low==1 && context->r[5].low==2
                && context->r[6].low==1 && context->r[29].low==initial_sp-32 && context->r[31].low==0x1005D0) {
                graph_startup_phase=4;graph_halfword_checkpoint=halfword_writes;context->r[2].low=0;return;
            }
            if(gs_imr_writes!=1 || gs_crt_calls || context->pc!=0x32E868 || context->r[4].low!=1
                    || context->r[5].low!=2 || context->r[6].low!=1 || context->r[29].low!=initial_sp-608
                    || context->r[31].low!=0x100274)throw UnsupportedSyscall(selector,context->pc);
            gs_crt_request={1,2,1};++gs_crt_calls;context->r[2].low=0;return;
        }
        if (code == 0 && selector == 0x7A && iop_reset_pending && reset_ready_supplied_) {
            if(reset_ready_checkpoint_reads || reset_clear_writes!=4 || context->pc!=0x32F078
                    || context->r[4].low!=4 || context->r[29].low!=initial_sp-48
                    || context->r[31].low!=0x338118)throw UnsupportedSyscall(selector,context->pc);
            // Explicit reference checkpoint replay: this caller tests only bit 0x40000.
            // This does not model full SMFLAG or execute/complete the native IOP boot.
            context->r[2].low=0x40000;++reset_ready_checkpoint_reads;return;
        }
        if (code == 0 && selector == 0x7A && sif_stop_calls) {
            if (!sif_reset_subaddr_supplied_ || sif_reset_subaddr_reads
                    || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000 || context->r[29].low != initial_sp - 224)
                throw UnsupportedSyscall(selector, context->pc);
            ++sif_reset_subaddr_reads; context->r[2].low = 0x19600;
            return;
        }
        if (code == 0 && selector == 0x7A && static_cast<std::uint32_t>(context->r[4].low) == 0x80000002) {
            if (!sif_rpc_supplied_ || sif_rpc_reads || sif_dma_submissions != 1)
                throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = 1;
            ++sif_rpc_reads;
            return;
        }
        if (code == 0 && selector == 0x7A) {
            if (!sif_command_supplied_ || sif_command_reads || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000
                    || dmac_enable_calls != 1) throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = static_cast<std::uint64_t>(static_cast<std::int64_t>(
                std::bit_cast<std::int32_t>(sif_command_word_)));
            ++sif_command_reads;
            return;
        }
        if (code == 0 && selector == 0x4A) {
            if (!osd_readback_supplied || !((osd_config_reads == 1 && osd_config_sets == 0)
                    || (osd_config_reads == 2 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            if (address & 3) throw UnsupportedGuestMemory(address, 4);
            checkMemory(rdram, address, 4);
            // Capture established only the same-value write; mutations fail.
            if (read32(rdram, address) != osd_config_word) throw UnsupportedSyscall(selector, context->pc);
            ++osd_config_sets;
            return;
        }
        if (code == 0 && selector == 0x4B) {
            if (!osd_config_supplied || (osd_config_reads && !(osd_readback_supplied && osd_config_reads == 1 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            if (address & 3) throw UnsupportedGuestMemory(address, 4);
            checkMemory(rdram, address, 4);
            write32(rdram, address, osd_config_word);
            ++osd_config_reads;
            return; // SDK declares void; preserve the result register.
        }
        if (code == 0 && selector == 0x32 && active_ee_stack && current_thread_id==3) {
            if(context->pc!=0x32EB88 || context->r[29].low!=0x3A6C80
                || context->r[28].low!=0x38BDF0 || context->r[16].low!=0x3A6D00
                || read32(rdram,0x3A6C90)!=0x100250
                || read32(rdram,0x3A6D00)!=3 || read32(rdram,0x3A6D08)!=0x3A6D18
                || read32(rdram,0x3A6D18)!=0x80000704 || read32(rdram,0x3A6D1C)!=0x3153F0
                || read32(rdram,0x3A6D20)!=0x3A6CC0)
                throw UnsupportedSyscall(selector,context->pc);
            if(ee_queued_sound_rpc) {
                if(sound_thread_waiting || !queued_sounds_bind.pending) throw UnsupportedSyscall(selector,context->pc);
                sound_thread_waiting=true;threads.at(1).ready=false;threads.at(1).waiting=true;threads.at(1).context=*context;
            } else ++ee_sound_probe_waits;
            ee_sound_probe_frontier=*context;
            active_ee_stack->yield();
            // No owned wake transaction exists yet.
            throw std::runtime_error("Unvalidated EE sound wake");
        }
        if (code == 0 && selector == 0x29 && context->r[4].low==1) {
            if(ee_sound_dispatch_probe && context->r[5].low==24) probeReadySoundThread(rdram,context);
            if(ee_queued_sound_rpc && context->r[5].low==24 && sound_thread_waiting) {
                dispatchReadyStartupWorkers(rdram,context);return;
            }

            // Only the main thread is runnable; created threads remain dormant.
            const bool startup_priority_two=texture_gif.phase==8 && timer1_setup_writes==1 && context->pc==0x32EAF8
                && context->r[29].low==initial_sp-32 && context->r[31].low==0x1005EC && context->r[5].low==2;
            if (!thread_initialized || current_thread_id != 1 || context->r[4].low != 1 || (context->r[5].low != 1 && !startup_priority_two))
                throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = main_thread_priority;
            main_thread_priority = startup_priority_two?2u:1u;
            ++priority_changes;
            return;
        }
        if (code == 0 && selector == 0x2F) {
            if (!thread_initialized || !current_thread_id) throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = current_thread_id;
            return;
        }
        if (code == 0 && selector == 0x20) {
            const auto parameter = static_cast<std::uint32_t>(context->r[4].low);
            if (!thread_initialized || !heap_initialized || parameter % 4)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, parameter, 36);
            const auto entry = read32(rdram, parameter + 4);
            const auto stack = read32(rdram, parameter + 8);
            const auto size = read32(rdram, parameter + 12);
            const auto gp = read32(rdram, parameter + 16);
            const auto priority = read32(rdram, parameter + 20);
            if (!entry || entry % 4 || stack % 16 || size % 16 || size < 0x2C0 || priority >= 128)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, entry, 4);
            checkMemory(rdram, stack, size);
            checkMemory(rdram, gp, 4);
            const auto top = stack + size;
            const auto saved_context = top - 0x2A0;
            if (parameter < top && saved_context < parameter + 36)
                throw UnsupportedSyscall(selector, context->pc);
            // Reference model reserves main ID 1 and idle ID 256; no ID reuse/scheduler yet.
            if (threads.size() == 254) { context->r[2].low = UINT64_MAX; return; }
            const auto id = static_cast<std::uint32_t>(threads.size()) + 2;
            threads.push_back({id, entry, stack, size, gp, priority, heap_start, saved_context, top - 0x20});
            threads.back().current_priority=priority;
            // Pinned ThreadReset writes only these low words, preserving other context bytes.
            write32(rdram, saved_context + 28 * 16, gp);
            write32(rdram, saved_context + 29 * 16, top - 0x20);
            write32(rdram, saved_context + 30 * 16, top - 0x20);
            const auto* spec=id>=4 && id<=7 ? &startup_worker_specs[id-4] : nullptr;
            const bool startup_worker=spec && entry==spec->entry && stack==spec->stack && size==spec->size
                && context->r[31].low==spec->create_return && gp==0x38BDF0 && priority==2
                && cri_rpc_calls==37 && heap_calls==6 && main_thread_queries==1 && context->pc==0x32EA68
                && current_thread_id==1 && main_thread_priority==2;
            write32(rdram, saved_context + 31 * 16, (sound_thread_start_supplied && id==3 && entry==0x315380) || startup_worker ? thread_arguments[4] : 0x1FC03000);
            context->r[2].low = id;
            return;
        }
        if(code==0 && selector==0x30 && cri_rpc_calls==37 && main_thread_queries==1
            && (context->r[4].low==4 || context->r[4].low==7) && !threads.empty() && threads.back().ready) {
            const auto id=static_cast<unsigned>(context->r[4].low);const unsigned phase=id==4?0:1;
            const auto out=static_cast<unsigned>(context->r[5].low);
            if(startup_ready_queries!=phase || startup_worker_starts!=(id==4?1u:4u) || startup_worker_priority_changes!=(id==4?0u:4u)
                || threads.size()!=id-1 || threads.back().id!=id || current_thread_id!=1 || main_thread_priority!=2 || context->pc!=0x32EB68
                || context->r[31].low!=0x2EE494 || context->cop0_status!=0x10001 || out!=initial_sp-288)
                throw UnsupportedSyscall(selector,context->pc);
            const auto& t=threads.back();checkMemory(rdram,out,48);
            const std::array<std::uint32_t,12> info={2,t.entry,t.stack_base,t.stack_size,t.gp,t.initial_priority,t.current_priority,0,0,0,0,0};
            for(unsigned n=0;n<12;++n)write32(rdram,out+n*4,info[n]);
            context->r[2].low=2;++startup_ready_queries;return;
        }
        if(code==0 && selector==0x37 && cri_rpc_calls==37 && main_thread_queries==1 && (context->r[4].low==4 || context->r[4].low==7)) {
            const auto id=static_cast<unsigned>(context->r[4].low);const unsigned phase=id==4?0:1;
            if(startup_ready_queries!=phase+1 || startup_worker_suspends!=phase || startup_worker_starts!=(id==4?1u:4u)
                || threads.size()!=id-1 || threads.back().id!=id || !threads.back().ready || threads.back().suspended
                || current_thread_id!=1 || main_thread_priority!=2 || context->pc!=0x32EBD8
                || context->r[31].low!=0x2EE4B4 || context->cop0_status!=0x10001)
                throw UnsupportedSyscall(selector,context->pc);
            threads.back().ready=false;threads.back().suspended=true;
            ++startup_worker_suspends;context->r[2].low=id;return; // Captured SuspendThread; the child has not executed.
        }
        if(code==0 && selector==0x30 && cri_rpc_calls==37 && main_thread_queries==1 && (context->r[4].low>=4 && context->r[4].low<=7)) {
            const auto id=static_cast<unsigned>(context->r[4].low);const auto out=static_cast<unsigned>(context->r[5].low);
            if(heap_calls!=6 || current_thread_id!=1 || main_thread_priority!=2 || threads.size()!=id-1 || startup_worker_queries!=id-4
                || context->pc!=0x32EB68 || context->cop0_status!=1 || out!=initial_sp-320 || threads.back().ready
                || threads.back().id!=id || threads.back().entry!=startup_worker_specs[id-4].entry || threads.back().current_priority!=2)
                throw UnsupportedSyscall(selector,context->pc);
            const auto& t=threads.back();checkMemory(rdram,out,48);
            const std::array<std::uint32_t,12> info={0x10,t.entry,t.stack_base,t.stack_size,t.gp,t.initial_priority,t.current_priority,0,0,0,0,0};
            for(unsigned n=0;n<12;++n)write32(rdram,out+n*4,info[n]);
            context->r[2].low=0x10;++startup_worker_queries;return;
        }
        if(code==0 && selector==0x22 && cri_rpc_calls==37 && main_thread_queries==1 && (context->r[4].low>=4 && context->r[4].low<=7)) {
            const auto id=static_cast<unsigned>(context->r[4].low);
            if(heap_calls!=6 || current_thread_id!=1 || main_thread_priority!=2 || threads.size()!=id-1 || startup_worker_queries!=id-3
                || startup_worker_starts!=id-4 || context->pc!=0x32EA88 || context->cop0_status!=0x10001 || context->r[5].low || threads.back().ready)
                throw UnsupportedSyscall(selector,context->pc);
            auto& t=threads.back();checkMemory(rdram,t.context_address,512);R5900Context child{};
            for(unsigned n=0;n<32;++n)std::memcpy(&child.r[n],memory_.data()+t.context_address+n*16,16);
            if(child.r[28].low!=0x38BDF0 || child.r[29].low!=t.initial_sp || child.r[31].low!=0x100250)
                throw UnsupportedSyscall(selector,context->pc);
            child.r[0]={0,0};child.r[4].low=0;child.pc=t.entry;child.cop0_status=context->cop0_status;
            t.context=child;t.ready=true;++startup_worker_starts;context->r[2].low=id;return;
        }
        if(code==0 && selector==0x29 && cri_rpc_calls==37 && main_thread_queries==1 && (context->r[4].low>=4 && context->r[4].low<=7)) {
            const auto id=static_cast<unsigned>(context->r[4].low);const auto priority=startup_worker_specs[id-4].priority;
            if(current_thread_id!=1 || main_thread_priority!=2 || startup_worker_starts!=id-3 || startup_worker_priority_changes!=id-4
                || threads.size()!=id-1 || (id==4 ? (threads.back().ready || !threads.back().suspended) : (!threads.back().ready || threads.back().suspended))
                || threads.back().current_priority!=2 || context->r[5].low!=priority
                || context->pc!=0x32EAF8 || context->r[31].low!=startup_worker_specs[id-4].priority_return)
                throw UnsupportedSyscall(selector,context->pc);
            threads.back().current_priority=priority;++startup_worker_priority_changes;context->r[2].low=2;return;
        }
        if(code==0 && selector==0x30 && cri_rpc_calls==37 && context->r[4].low==1) {
            const auto out=static_cast<std::uint32_t>(context->r[5].low);
            if(main_thread_queries || heap_calls!=6 || !thread_initialized || current_thread_id!=1 || main_thread_priority!=2
                || threads.size()!=2 || (ee_queued_sound_rpc ? (!sound_thread_waiting || threads[1].ready || queued_sound_completions!=1) : !threads[1].ready) || threads[1].id!=3 || semaphores.size()!=10
                || context->pc!=0x32EB68 || context->r[31].low!=0x2EE9FC || context->r[29].low!=out || out!=initial_sp-144
                || context->cop0_status!=0x10001 || thread_arguments[0]!=0x38BDF0
                || stack_base+thread_arguments[2]!=ram_size-4096)
                throw UnsupportedSyscall(selector,context->pc);
            checkMemory(rdram,out,48);
            // This exact caller consumes status return and current_priority only.
            // Preserve other descriptor bytes; their BIOS live-state meaning is unmodeled.
            write32(rdram,out,1);write32(rdram,out+24,main_thread_priority);
            context->r[2].low=1;++main_thread_queries;return;
        }
        if(code==0 && selector==0x30 && sound_thread_checkpoint) {
            const auto out=static_cast<std::uint32_t>(context->r[5].low);
            if(sound_thread_queries || context->pc!=0x32EB68 || context->r[4].low!=3 || threads.size()!=2
                || threads[1].ready || out!=initial_sp-256 || context->cop0_status!=1)
                throw UnsupportedSyscall(selector,context->pc);
            const auto& t=threads[1];checkMemory(rdram,out,48);
            const std::array<std::uint32_t,12> info={0x10,t.entry,t.stack_base,t.stack_size,t.gp,t.initial_priority,t.initial_priority,0,0,0,0,0};
            for(unsigned n=0;n<info.size();++n)write32(rdram,out+n*4,info[n]);
            context->r[2].low=0x10;++sound_thread_queries;return;
        }
        if(code==0 && selector==0x22 && sound_thread_checkpoint) {
            if(sound_thread_starts || sound_thread_queries!=1 || context->pc!=0x32EA88 || context->r[4].low!=3 || context->r[5].low
                || current_thread_id!=1 || main_thread_priority!=2 || threads.size()!=2 || threads[1].ready || context->cop0_status!=0x10001)
                throw UnsupportedSyscall(selector,context->pc);
            auto& t=threads[1];
            if(t.entry!=0x315380 || t.stack_base!=0x3A5CC0 || t.stack_size!=0x1000 || t.gp!=0x38BDF0 || t.initial_priority!=5)
                throw UnsupportedSyscall(selector,context->pc);
            checkMemory(rdram,t.context_address,512);
            R5900Context child{};
            for(unsigned n=0;n<32;++n)std::memcpy(&child.r[n],memory_.data()+t.context_address+n*16,16);
            child.r[0]={0,0};child.r[4].low=0;child.pc=t.entry;child.cop0_status=context->cop0_status;
            if(child.r[28].low!=t.gp || child.r[29].low!=t.initial_sp || child.r[31].low!=thread_arguments[4] || thread_arguments[4]!=0x100250)
                throw UnsupportedSyscall(selector,context->pc);
            t.context=child;t.ready=true;++sound_thread_starts;context->r[2].low=t.id;return;
        }
        if (code == 0 && selector == 0x14) {
            // Observed cause only; explicit mask checkpoint avoids a fabricated BIOS default.
            if (!intc_mask_supplied_ || static_cast<std::uint32_t>(context->r[4].low) != 11)
                throw UnsupportedSyscall(selector, context->pc);
            constexpr std::uint32_t bit = 1u << 11;
            context->r[2].low = (intc_mask & bit) ? 0 : 1;
            intc_mask |= bit;
            ++intc_enable_calls;
            return;
        }
        if (code == 0 && selector == 0x10) {
            const auto cause = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            // Observed startup contract only. No interrupt delivery or removal yet.
            if (cause != 11 || !address || address % 4 || context->r[6].low != 0 || context->r[7].low != 0)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, address, 4);
            const auto id = addIntcHandler(cause, address, 0, static_cast<std::uint32_t>(context->r[28].low));
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x12) {
            const auto channel = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            if (channel != 5 || !address || address % 4 || context->r[6].low != 0)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, address, 4);
            if (dmac_handlers.size() == 128) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            if(post_sif.supplied && gif_dma.completions==2) {
                if(address!=0x331DF0 || !dmac_handlers.empty())throw UnsupportedSyscall(selector,context->pc);
                post_sif.handler();
            }
            dmac_handlers.push_back({channel, address, static_cast<std::uint32_t>(context->r[7].low),
                                     static_cast<std::uint32_t>(context->r[28].low)});
            const auto id = static_cast<std::uint32_t>(dmac_handlers.size());
            dmac_handler_order.insert(dmac_handler_order.begin(), id);
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x17) {
            // Reached SIF shutdown: toggle only the recorded channel-5 mask bit.
            if (context->r[4].low != 5 || scmd_mmode_calls != 1 || dmac_handlers.size() != 1)
                throw UnsupportedSyscall(selector, context->pc);
            const auto bit = 0x10000u << 5;
            const bool changed = (dmac_mask & bit) != 0;
            dmac_mask &= ~bit;
            ++dmac_disable_calls;
            context->r[2].low = changed ? 1 : 0;
            return;
        }
        if (code == 0 && selector == 0x13) {
            // Sole registered dormant handler; no dispatch or wait queue exists.
            if (context->r[4].low != 5 || context->r[5].low != 1 || dmac_disable_calls != 1
                    || dmac_mask != 0x800000 || dmac_handlers.size() != 1
                    || dmac_handlers[0].channel != 5 || dmac_handler_order != std::vector<std::uint32_t>{1})
                throw UnsupportedSyscall(selector, context->pc);
            dmac_handlers.clear(); dmac_handler_order.clear(); ++dmac_removals;
            context->r[2].low = 0; // Remaining handlers on this channel.
            return;
        }
        if (code == 0 && selector == 0x16) {
            // Captured mask bits only. No status acknowledgements, DMA or interrupt delivery.
            if (context->r[4].low != 5 || dma_status_reads != 1 || dma5_control_reads != 1
                    || dmac_handlers.empty()) throw UnsupportedSyscall(selector, context->pc);
            if(post_sif.supplied && gif_dma.completions==2)post_sif.enable();
            const auto bit = 0x10000u << 5;
            const bool changed = !(dmac_mask & bit);
            dmac_mask |= bit;
            ++dmac_enable_calls;
            context->r[2].low = changed ? 1 : 0;
            return;
        }
        if (code == 0 && selector == 0x74) {
            const auto number = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            const bool timer_interrupt = number == 0x12C;
            if ((!timer_interrupt && number >= syscall_handlers.size()) || address % 4
                    || (timer_interrupt && (timer3_interrupt_handler || address < 0x80000000)))
                throw UnsupportedSyscall(selector, context->pc);
            if (address >= 0x80000000) {
                // Register only a word actually written into the explicitly enabled patch.
                // Registration does not make these bytes a compiled native function.
                const bool primary_patch = address >= kernel_patch_start_ && address < kernel_patch_end_;
                bool startup_payload = address == 0x80075000 && startup_kernel_word_writes == 0x330 / 4;
                if (!startup_kernel_payload_.empty() && startup_kernel_word_writes == 0x330 / 4
                        && address >= 0x80075000 && address < 0x80075330) {
                    // Exact selector/address pairs from the fully validated copied table.
                    for (std::size_t offset = 0x300; offset < 0x330; offset += 8) {
                        std::uint32_t table_selector, table_address;
                        std::memcpy(&table_selector, startup_kernel_payload_.data() + offset, 4);
                        std::memcpy(&table_address, startup_kernel_payload_.data() + offset + 4, 4);
                        if (number == table_selector && address == table_address) startup_payload = true;
                    }
                }
                if (memory_.empty() || rdram != memory_.data() || (!primary_patch && !startup_payload)
                        || !kernel_patch_words_.contains(address - 0x80000000))
                    throw UnsupportedGuestMemory(address, 4);
            } else if (address) checkMemory(rdram, address, 4);
            if (timer_interrupt) {
                // Pinned Play! maps this extended selector to INTC line 12.
                // Record one startup registration/enable request; no interrupt delivery.
                if (addIntcHandler(12, address, 0, 0) == UINT64_MAX)
                    throw std::runtime_error("Timer 3 interrupt handler pool exhausted");
                timer3_interrupt_handler = address;
                timer3_interrupt_enable_requested = true;
                ++extended_registrations;
            } else syscall_handlers[number] = address;
            ++syscall_registrations;
            // PS2SDK declares void; preserve v0 rather than inventing a result.
            return;
        }
        if (code == 0 && selector == 0x3C) {
            // Bootstrap model follows pinned Play! sc_SetupThread; no scheduler yet.
            const auto stack = static_cast<std::uint32_t>(context->r[5].low);
            const auto size = static_cast<std::uint32_t>(context->r[6].low);
            const auto args = static_cast<std::uint32_t>(context->r[7].low);
            constexpr std::uint32_t top = static_cast<std::uint32_t>(ram_size) - 4096;
            if (thread_initialized || stack != UINT32_MAX || size < 0x2A0 || size > top
                || size % 16 || args % 4)
                throw std::runtime_error("Unsupported bootstrap SetupThread contract");
            checkMemory(rdram, args, 8);
            if (startup_zero_end_) {
                for (auto address = startup_zero_start_; address < startup_zero_end_; ++address)
                    if (memory_[address]) throw std::runtime_error("Startup zero region was not cleared before SetupThread");
                startup_zero_verified = true;
                startup_quadword_writes = quadword_writes;
                startup_byte_writes = byte_writes;
            }
            stack_base = top - size;
            initial_sp = top - 0x2A0;
            // Explicit empty launch: argc=0, argv[0]=nullptr.
            std::memset(memory_.data() + args, 0, 8);
            thread_arguments = {static_cast<std::uint32_t>(context->r[4].low), stack, size,
                                args, static_cast<std::uint32_t>(context->r[8].low)};
            context->r[2].low = initial_sp;
            current_thread_id = 1;
            thread_initialized = true;
            return;
        }
        if (code == 0 && selector == 0x3D) {
            // Pinned Play! default heap end is the initialized main thread's stack base.
            const auto start = static_cast<std::uint32_t>(context->r[4].low);
            const auto size = static_cast<std::uint32_t>(context->r[5].low);
            if (!thread_initialized || heap_initialized || size != UINT32_MAX || start >= stack_base)
                throw std::runtime_error("Unsupported bootstrap SetupHeap contract");
            checkMemory(rdram, start, 1);
            heap_start = start;
            heap_end = stack_base;
            context->r[2].low = heap_end;
            heap_initialized = true;
            return;
        }
        if(code==0 && selector==0x3E) {
            if(!thread_initialized || !heap_initialized || current_thread_id!=1)throw UnsupportedSyscall(selector,context->pc);
            context->r[2].low=heap_end;++heap_end_queries;return;
        }
        if(code==0 && selector==0x7C && sound_rpc_calls==1 && native_module_loads==14) {
            if(context->pc!=0x32F098 || context->r[4].low!=16 || context->r[5].low!=context->r[29].low)
                throw UnsupportedSyscall(selector,context->pc);
            const auto pointer=read32(rdram,static_cast<std::uint32_t>(context->r[5].low));
            std::string message;bool terminated=false;
            for(unsigned n=0;n<4096;++n) {
                const auto ch=read8(rdram,pointer+n);if(!ch){terminated=true;break;}message+=static_cast<char>(ch);
            }
            if(!terminated)throw UnsupportedSyscall(selector,context->pc);
            ee_console_messages.push_back(std::move(message));context->r[2].low=UINT64_MAX;return;
        }
        if (code == 0 && selector == 0x40) {
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            checkMemory(rdram, address, 24);
            if (address % 4) throw std::runtime_error("Unaligned CreateSema parameters");
            // Input count/wait_threads are output fields, not initial state.
            const auto maximum = read32(rdram, address + 4);
            const auto initial = read32(rdram, address + 8);
            const auto attributes = read32(rdram, address + 16);
            const auto option = read32(rdram, address + 20);
            // PS2SDK synchronous RPC leaves attr uninitialized; the pinned Play!
            // model reads count/max/option only. Permit this field solely for the
            // reached first CD-init call, whose local BIOS capture returns ID 3.
            const bool rpc_call_sema = sif_call_sema_supplied_ && sif_bind_callbacks == 1 && semaphore_deletions == 1
                && semaphores.size() == 3 && address == initial_sp - 400
                && maximum == 1 && initial == 0 && option == 0x3838E8;
            const bool cd_sema = cd_semaphores_supplied_ && cd_init_calls == 1 && semaphore_deletions == 2
                && cd_semaphores_created < 4 && semaphores.size() == 3 + cd_semaphores_created
                && address == initial_sp - 304 && maximum == 1
                && initial == (cd_semaphores_created == 3 ? 0u : 1u)
                && option == 0x3825B8 + cd_semaphores_created * 16;
            const bool scmd_sema = scmd_bind_supplied_ && cd_semaphores_created == 4 && !scmd_bind_calls
                && semaphores.size() == 7 && address == initial_sp - 288 && maximum == 1
                && initial == 0 && option == 0x3838D8 && attributes == 2;
            const bool mmode_sema = scmd_mmode_supplied_ && scmd_bind_calls == 1 && !scmd_mmode_calls
                && semaphores.size() == 7 && address == initial_sp - 304 && maximum == 1
                && initial == 0 && option == 0x3838E8 && attributes == 0x370000;
            // Same reached RPC helper as before reboot; attr remains unused stack residue.
            const bool fresh_bind_sema = post_sif.rpc_initialized && semaphores.size()==7
                && address==initial_sp-320 && maximum==1 && initial==0 && option==0x3838D8;
            const bool fresh_call_sema=post_cd.supplied && post_cd.binds==2 && !post_cd.calls && semaphores.size()==7
                && address==initial_sp-400 && maximum==1 && initial==0 && option==0x3838E8;
            const bool fresh_scmd_sema=post_cd.scmd_supplied && post_cd.calls==1 && semaphores.size()==7 && maximum==1 && initial==0
                && ((!post_cd.scmd_binds && address==initial_sp-288 && option==0x3838D8)
                    || (post_cd.scmd_binds==1 && !post_cd.modes && address==initial_sp-304 && option==0x3838E8));
            const bool fresh_loader_bind_sema=post_cd.modes==1 && semaphores.size()==7
                && address==initial_sp-384 && maximum==1 && initial==0 && option==0x3838D8;
            const bool fresh_loader_call_sema=post_loadfile.binds==1 && semaphores.size()==7 && maximum==1 && initial==0
                && option==0x3838E8 && (address==initial_sp-(post_loadfile.versions?384:464)
                    || (heap_dma_polls==1 && native_module_loads==13 && address==initial_sp-496 && attributes==0x3AC840)
                    || (heap_dma_polls==1 && native_module_loads==14 && !heap_frees && address==initial_sp-400 && attributes==0x3B0000));
            const bool mc_call_sema=mc_rpc_binds==1 && !mc_rpc_calls && semaphores.size()==10 && address==initial_sp-400 && maximum==1 && initial==0 && option==0x3838E8;
            const bool dbc_init_sema=dbc_rpc_binds==4 && dbc_rpc_calls==1 && semaphores.size()==7 && address==initial_sp-288 && maximum==127 && initial==1 && option==0x381C28;
            const bool dbc_call_sema=sounds_rpc_calls==1 && ((dbc_rpc_binds==1 && !dbc_rpc_calls && semaphores.size()==7) || (dbc_rpc_binds==4 && dbc_rpc_calls>=1 && dbc_rpc_calls<4 && semaphores.size()==8)) && address==initial_sp-(dbc_rpc_calls>=2?512:528) && maximum==1 && initial==0 && option==0x3838E8;
            const bool dbc_bind_sema=sounds_rpc_calls==1 && dbc_rpc_binds<4 && semaphores.size()==7 && address==initial_sp-400 && maximum==1 && initial==0 && option==0x3838D8;
            const bool sounds_bind_sema=sound_thread_starts==1 && !sounds_rpc_binds && native_module_loads==14 && heap_calls==2
                && semaphores.size()==7 && address==initial_sp-176 && maximum==1 && initial==0 && option==0x3838D8;
            const bool sound_call_sema=sound_rpc_binds==1 && !sound_rpc_calls && native_module_loads==14 && heap_calls==2
                && semaphores.size()==7 && address==initial_sp-592 && maximum==1 && initial==0 && option==0x3838E8;
            const bool cri_call_sema=cri_rpc_binds==1 && cri_rpc_calls<=41 && heap_calls==(cri_rpc_calls>=36?6u:cri_rpc_calls==35?5u:cri_rpc_calls>=3?4u:3u)
                && semaphores.size()==10 && address==initial_sp-(cri_rpc_calls>=36?(cri_rpc_calls==36?448u:512u):cri_rpc_calls==35?448u:cri_rpc_calls>=3?(cri_rpc_calls&1?544u:592u):512u) && maximum==1 && initial==0
                && option==0x3838E8 && context->r[17].low==0x355C70
                && read32(rdram,0x3A97E0)==(cri_rpc_calls>=36?(cri_rpc_calls==36?0x40Au:0x408u):cri_rpc_calls==35?2u:cri_rpc_calls>=3?(cri_rpc_calls&1?0x422u:0x400u):cri_rpc_calls==2?0x40Cu:cri_rpc_calls==1?0x403u:2u);
            const bool fourth_heap_sema=((heap_calls==3 && cri_rpc_calls==3) || (heap_calls==4 && cri_rpc_calls==35) || (heap_calls==5 && cri_rpc_calls==36)) && disc_rpc_calls==6 && ncmd_read_calls==1
                && semaphores.size()==10 && address==initial_sp-(heap_calls>=4?416u:496u) && maximum==1 && initial==0 && option==0x3838E8
                && context->pc==0x32EC68 && context->r[31].low==0x332AD0 && context->r[17].low==0x3AC840;
            const bool third_heap_sema=heap_calls==2 && cri_rpc_binds==1 && disc_rpc_calls==6 && ncmd_read_calls==1 && semaphores.size()==10 && address==initial_sp-480 && maximum==1 && initial==0 && option==0x3838E8 && context->r[17].low==0x3AC840;
            const bool second_heap_sema=heap_calls==1 && heap_frees==1 && texture_gif.phase==8 && native_module_loads==14
                && semaphores.size()==7 && address==initial_sp-288 && maximum==1 && initial==0 && option==0x3838E8;
            const bool heap_bind_sema=heap_bind_supplied && native_module_loads==13 && !heap_binds && post_loadfile.versions==1
                && semaphores.size()==7 && address==initial_sp-192 && maximum==1 && initial==0 && option==0x3838D8
                && read32(rdram,0x3AC864)==0;
            // The actual bind helper writes only max/init/option. An isolated
            // retail return confirms semaphore 10 with different unused attr residue.
            const bool disc_bind_sema=mc_rpc_calls==2 && mc_rpc_binds==1 && semaphores.size()==10
                && !disc_bind_semaphores && address==initial_sp-592 && maximum==1 && initial==0 && option==0x3838D8
                && context->pc==0x32EC68 && context->r[31].low==0x332814
                && context->r[17].low==0x3A8780 && static_cast<std::uint32_t>(context->r[19].low)==0x80000597
                && read32(rdram,0x3A8780)==0x203A97C0 && read32(rdram,0x3A8784)==42;
            const bool disc_call_sema=disc_rpc_binds==1 && (disc_rpc_calls==0 || (disc_rpc_calls>=1 && disc_rpc_calls<6 && ncmd_read_calls==1)) && semaphores.size()==10 && address==initial_sp-(disc_rpc_calls?688:672)
                && maximum==1 && initial==0 && option==0x3838E8 && context->pc==0x32EC68 && context->r[31].low==0x332AD0
                && context->r[17].low==0x3A8780 && context->r[19].low==1 && read32(rdram,0x3A8784)==(disc_rpc_calls?46u+disc_rpc_calls:43u);
            const bool ncmd_bind_sema=disc_rpc_calls==1 && !ncmd_bind_semaphores && semaphores.size()==10
                && address==initial_sp-608 && maximum==1 && initial==0 && option==0x3838D8
                && context->pc==0x32EC68 && context->r[31].low==0x332814 && context->r[17].low==0x369590
                && static_cast<unsigned>(context->r[19].low)==0x80000595 && read32(rdram,0x369594)==44;
            const bool ncmd_ready_sema=ncmd_rpc_binds==1 && !ncmd_ready_calls && semaphores.size()==10
                && address==initial_sp-624 && maximum==1 && initial==0 && option==0x3838E8
                && context->pc==0x32EC68 && context->r[31].low==0x332AD0 && context->r[17].low==0x369590
                && context->r[19].low==1 && read32(rdram,0x369594)==45;
            // After main's admitted priority: the SIF RPC library's own temporary call (RA 0x332AD0) or
            // bind (RA 0x332814) semaphore; attr is uninitialized stack residue in that library.
            const bool admitted_rpc_sema=main_priority_admissions && context->pc==0x32EC68 && maximum==1 && initial==0
                && ((context->r[31].low==0x332AD0 && option==0x3838E8) || (context->r[31].low==0x332814 && option==0x3838D8));
            // The C library's lazily created stdio lock ("SceStdioFioSema", count 1) after admission.
            const bool stdio_sema=main_priority_admissions && context->pc==0x32EC68 && context->r[31].low==0x333770
                && maximum==1 && initial==1 && option==0x383988 && read32(rdram,0x36AB24)==0xFFFFFFFF;
            // Its per-call file semaphores (count 0), named by option: "SceStdioOpenSema" 0x3839A0,
            // "...CloseSema" 0x3839B8, "...LseekSema" 0x3839D0, "...ReadSema" 0x3839E8.
            const bool stdio_open_sema=main_priority_admissions && context->pc==0x32EC68 && stdioCallSema(option)
                && context->r[31].low>=0x333D00 && context->r[31].low<0x334800 && maximum==1 && initial==0;
            if(ncmd_bind_sema)++ncmd_bind_semaphores;
            if (disc_bind_sema) ++disc_bind_semaphores;
            if (attributes != 0 && !admitted_rpc_sema && !stdio_sema && !stdio_open_sema && !ncmd_ready_sema && !ncmd_bind_sema && !disc_bind_sema && !disc_call_sema && !mc_call_sema && !dbc_init_sema && !dbc_call_sema && !dbc_bind_sema && !sounds_bind_sema && !sound_call_sema && !cri_call_sema && !fourth_heap_sema && !third_heap_sema && !second_heap_sema && !heap_bind_sema && !fresh_loader_call_sema && !fresh_loader_bind_sema && !fresh_scmd_sema && !fresh_call_sema && !fresh_bind_sema && !rpc_call_sema && !cd_sema && !scmd_sema && !mmode_sema) {
                if (sif_bind_callbacks) throw UnsupportedSyscall(selector, context->pc);
                throw std::runtime_error("Unsupported semaphore attributes");
            }
            if (cd_sema) ++cd_semaphores_created;
            if (rpc_call_sema) { ++rpc_call_semaphores; rpc_call_sema_attributes = attributes; }
            if (!maximum || maximum > INT32_MAX || initial > maximum || semaphores.size() == 256) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            const auto id = static_cast<std::uint32_t>(semaphores.size());
            semaphores.push_back({initial, maximum, option});
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x45) {
            // Pinned nonblocking PollSema: never sleeps or reschedules.
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            ++semaphore_polls;
            if (id >= semaphores.size() || semaphores[id].count == 0) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            --semaphores[id].count;
            if(id==8 && semaphores[id].option==0x3824C8 && context->pc==0x32ECB8 && context->r[31].low==0x31DE18)++mc_lock_polls;
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x41) {
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            if (id >= semaphores.size()) { context->r[2].low = UINT64_MAX; return; }
            // Observed completed bind cleanup only, with no waiting threads.
            const bool bind_cleanup = sif_bind_callbacks == 1 && semaphore_waits == 2
                && semaphore_deletions == 0 && semaphores[id].option == 0x3838D8;
            const bool cd_cleanup = cd_init_calls == 1 && semaphore_waits == 3
                && semaphore_deletions == 1 && semaphores[id].option == 0x3838E8;
            const bool scmd_cleanup = scmd_bind_calls == 1 && semaphore_waits == 4 && semaphore_deletions == 2
                && id == 7 && semaphores.size() == 8 && semaphores[id].option == 0x3838D8;
            const bool mmode_cleanup = scmd_mmode_calls == 1 && semaphore_waits == 5 && semaphore_deletions == 3
                && id == 7 && semaphores.size() == 8 && semaphores[id].option == 0x3838E8;
            const bool fresh_cleanup=post_cd.supplied && ((id==7 && semaphores.size()==8) || (mc_rpc_binds && id==10 && semaphores.size()==11) || (dbc_rpc_binds==4 && dbc_rpc_calls>=2 && dbc_rpc_calls<=4 && id==8 && semaphores.size()==9))
                && semaphore_deletions==3+serviceOperations()
                && ((post_cd.binds && !post_cd.calls && semaphores[id].option==0x3838D8)
                    || (post_cd.calls==1 && !post_cd.scmd_binds && semaphores[id].option==0x3838E8)
                    || (post_cd.scmd_binds==1 && !post_cd.modes && semaphores[id].option==0x3838D8)
                    || (post_cd.modes==1 && !post_loadfile.binds && semaphores[id].option==0x3838E8)
                    || (post_loadfile.binds && !post_loadfile.versions && semaphores[id].option==0x3838D8)
                    || (post_loadfile.versions==1 && semaphores[id].option==0x3838E8)
                    || (heap_binds==1 && !heap_calls && semaphores[id].option==0x3838D8)
                    || (sound_rpc_binds==1 && !sound_rpc_calls && semaphores[id].option==0x3838D8)
                    || (sounds_rpc_binds==1 && semaphores[id].option==0x3838D8));
            // After main's admitted priority: the SIF RPC library deletes its own newest temporary
            // call/bind semaphore after completion.
            const bool admitted_rpc_cleanup=main_priority_admissions && id+1==semaphores.size() && context->pc==0x32EC78
                && (semaphores[id].option==0x3838E8 || semaphores[id].option==0x3838D8)
                && (context->r[31].low==0x332B3C || context->r[31].low==0x33287C);
            // The C library's completed open/close/lseek/read deletes its per-call semaphore (newest).
            const bool stdio_open_cleanup=main_priority_admissions && id+1==semaphores.size() && context->pc==0x32EC78
                && context->r[31].low>=0x333D00 && context->r[31].low<0x334800 && stdioCallSema(semaphores[id].option);
            if (semaphores[id].count != 0 || semaphores[id].maximum != 1
                    || (!admitted_rpc_cleanup && !stdio_open_cleanup && !(id == 3 && semaphores.size() == 4 && (bind_cleanup || cd_cleanup)) && !scmd_cleanup && !mmode_cleanup && !fresh_cleanup && !(disc_rpc_binds==1 && id==10 && semaphores.size()==11 && semaphores[id].option==((cri_rpc_binds && heap_calls>=3) || cri_rpc_calls?0x3838E8u:cri_rpc_binds?0x3838D8u:ncmd_ready_calls?0x3838E8u:ncmd_rpc_binds?0x3838D8u:disc_rpc_calls?0x3838E8u:0x3838D8u) && semaphore_deletions==2+serviceOperations())))
                throw UnsupportedSyscall(selector, context->pc);
            semaphores.pop_back();
            ++semaphore_deletions;
            context->r[2].low = id;
            return;
        }
        if (code == 0 && (selector == 0x44 || selector == 0x42
                || (selector == 0xFFFFFFBD && rpc_end_callback_active_))) {
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            if (id >= semaphores.size()) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            auto& semaphore = semaphores[id];
            if (selector == 0x44) {
                if(!semaphore.count && queued_sounds_bind.pending) finishQueuedSoundsBind(rdram,context);
                if (!semaphore.count) throw UnsupportedSyscall(selector, context->pc);
                --semaphore.count;
                ++semaphore_waits;
                if(id==8 && semaphore.option==0x3824C8 && context->pc==0x32ECA8 && (context->r[31].low==0x31DCFC || context->r[31].low==0x31D304))++mc_lock_waits;
                if(id==7 && semaphore.option==0x381C28 && context->pc==0x32ECA8 && context->r[31].low==0x3135B8)++dbc_lock_waits;
                if(context->pc==0x32ECA8 && context->r[31].low==0x3252F0)++ee_heap_lock_waits;
            } else {
                // No waiting threads exist in this bounded model. Overflow is unverified.
                if (semaphore.count >= semaphore.maximum) throw UnsupportedSyscall(selector, context->pc);
                ++semaphore.count;
                ++semaphore_signals;
                if(id==8 && semaphore.option==0x3824C8 && context->pc==0x32EC88 && (context->r[31].low==0x31DD34 || context->r[31].low==0x31D3E0))++mc_lock_signals;
                if(id==7 && semaphore.option==0x381C28 && context->pc==0x32EC88)++dbc_lock_signals;
                if(context->pc==0x32EC88 && texture_gif.phase==8 && id==read32(rdram,0x36AB58))++ee_heap_lock_signals;
            }
            context->r[2].low = id;
            return;
        }
        if (code != 0 || static_cast<std::uint32_t>(context->r[3].low) != 0x64)
            throw UnsupportedSyscall(static_cast<std::uint32_t>(context->r[3].low), context->pc);
        const auto operation = static_cast<std::uint32_t>(context->r[4].low);
        if (operation != 0 && operation != 2) throw UnsupportedSyscall(selector, context->pc);
        // Pinned PS2SDK: 0=writeback D-cache, 2=invalidate I-cache (void).
        // Stores are immediately visible and dispatch uses fixed native functions.
        // No guest instruction cache exists; this does not translate patched kernel code.
        if (operation == 2) ++instruction_cache_invalidations;
        ++cache_requests;
        syscall_resume_pc = context->pc;
    }

    bool dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target,
                             std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char*) {
        if (kind == GuestBranchKind::Return) {
            const auto expected = call_returns_.empty() ? expected_return : call_returns_.back();
            if (target != expected) throw std::runtime_error("Unexpected probe return target");
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

    void loadMemory(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != ram_size) throw std::runtime_error("Expected bounded 32 MiB guest image");
        memory_.assign(bytes.begin(), bytes.end());
    }

    std::span<std::uint8_t> memory() { return memory_; }

    // PS2SDK UNCACHED_SEG: reached low 32 MiB main-RAM alias only.
    // Enabled after the explicit RPCINIT query; no TLB/MMIO or cache state.
    std::uint32_t uncachedWordAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1 || address % 4) throw UnsupportedGuestMemory(address, 4);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 4);
        ++uncached_word_accesses;
        return physical;
    }
    unsigned uncached_word_accesses = 0;
    std::uint32_t uncachedByteAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 1);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 1);
        ++uncached_byte_accesses;
        return physical;
    }
    unsigned uncached_byte_accesses = 0;
    void write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value) {
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        memory_[address] = value;
        ++byte_writes;
    }

    std::uint16_t read16(std::uint8_t* rdram,std::uint32_t address) const {
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword load");
        std::uint16_t value;std::memcpy(&value,memory_.data()+address,2);return value;
    }
    void write16(std::uint8_t* rdram,std::uint32_t address,std::uint16_t value) {
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword store");
        std::memcpy(memory_.data()+address,&value,2);++halfword_writes;
    }
    ProbeGpr read128(std::uint8_t* rdram,std::uint32_t address) const {
        checkMemory(rdram,address,sizeof(ProbeGpr));
        if(address%16)throw std::runtime_error("Unsupported unaligned quadword load");
        ProbeGpr value;std::memcpy(&value,memory_.data()+address,sizeof(value));return value;
    }
    void write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value) {
        checkMemory(rdram, address, sizeof(value));
        if (address % 16) throw std::runtime_error("Unsupported unaligned quadword store");
        std::memcpy(memory_.data() + address, &value, sizeof(value));
        ++quadword_writes;
    }

    void write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
        address = uncachedWordAddress(rdram, address);
        if(address==0x10003000 && vif_startup.phase==5 && value==1 && !vif_startup.pending_mask) {
            checkMemory(rdram,0,0);
            if(gif_dma.starts!=2 || gif_dma.completions!=2 || gif_dma.qwc)throw UnsupportedGuestMemory(address,4);
            ++vif_startup.gif_resets;vif_startup.phase=6;return; // Idle GIF reset, not GS VRAM/drawing-state reset.
        }
        // The generated DMAC setup writes zero to STAT in its return delay slot.
        // STAT is W1C/toggle, so zero preserves the existing interrupt mask.
        if(address==0x1000E010 && dmac_startup_writes==2 && value==0 && heap_frees==1) {
            checkMemory(rdram,0,0);++dmac_startup_writes;return;
        }
        // Observed Timer 2 startup store only; no counter advancement or scheduling.
        if (address == 0xB0001010 && timer2_mode_supplied_ && timer2_mode_reads == 1
                && !timer2_mode_writes && value == 0x382) {
            checkMemory(rdram, 0, 0);
            timer2_mode_after_write = (timer2_mode_ & 0xC00 & ~value) | (value & 0x3FF);
            ++timer2_mode_writes;
            ++word_writes;
            return;
        }
        if (!startup_kernel_payload_.empty() && address >= 0x80075000 && address < 0x80075330) {
            if (memory_.empty() || rdram != memory_.data() || address % 4)
                throw UnsupportedGuestMemory(address, 4);
            const auto offset = address - 0x80075000;
            std::uint32_t expected;
            std::memcpy(&expected, startup_kernel_payload_.data() + offset, 4);
            if (value != expected || kernel_patch_words_.contains(address - 0x80000000))
                throw std::runtime_error("Startup kernel payload store differs or repeats");
            kernel_patch_words_[address - 0x80000000] = value;
            ++startup_kernel_word_writes;
            ++kernel_word_writes;
            ++word_writes;
            return;
        }
        if (address >= kernel_patch_start_ && address < kernel_patch_end_) {
            if (!kernel_patch_end_ || memory_.empty() || rdram != memory_.data() || address % 4
                    || kernel_patch_end_ - address < 4) throw UnsupportedGuestMemory(address, 4);
            kernel_patch_words_[address - 0x80000000] = value;
            ++kernel_word_writes;
            ++word_writes;
            return;
        }
        checkMemory(rdram, address, 4);
        if (address % 4) throw std::runtime_error("Unsupported unaligned word store");
        std::memcpy(memory_.data() + address, &value, 4);
        ++word_writes;
    }

    // Generated doubleword accesses: addresses in the EE register window take the same checked
    // MMIO path as constant-address accesses; everything else is main RAM.
    static bool mmioWindow(std::uint32_t address) {return address>=0x10000000 && address<0x14000000;}
    void guestWrite64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
        if(mmioWindow(address))Store64(rdram,context,address,value);else write64(rdram,address,value);
    }
    std::uint64_t guestRead64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        return mmioWindow(address)?Load64(rdram,context,address):read64(rdram,address);
    }
    void write64(std::uint8_t* rdram, std::uint32_t address, std::uint64_t value) {
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword store");
        std::memcpy(memory_.data() + address, &value, 8);
        ++doubleword_writes;
    }
    std::uint8_t read8(std::uint8_t* rdram, std::uint32_t address) {
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        return memory_[address];
    }
    std::uint64_t read64(std::uint8_t* rdram, std::uint32_t address) {
        // Generated bounded searches copy their owned results through the uncached EE alias.
        if(disc_rpc_calls>=1 && disc_rpc_calls<=6 && address>=0x203A8600 && address<=0x203A8618 && !(address&7))address-=0x20000000;
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword read");
        std::uint64_t value;
        std::memcpy(&value, memory_.data() + address, 8);
        return value;
    }

    static constexpr std::size_t kernel_snapshot_size = 0x80004;
    void loadKernelSnapshot(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != kernel_snapshot_size) throw std::runtime_error("Kernel snapshot must cover physical [0, 0x80004)");
        if (kernel_patch_end_) throw std::runtime_error("Cannot replace a kernel baseline with enabled patches");
        kernel_snapshot_.assign(bytes.begin(), bytes.end());
    }

    void enableStartupKernelPayload(std::span<const std::uint8_t> expected) {
        if (!kernel_patch_end_ || kernel_snapshot_.empty() || !startup_kernel_payload_.empty() || expected.size() != 0x330)
            throw std::runtime_error("Invalid startup kernel payload capability");
        if (kernel_patch_start_ < 0x80075330 && 0x80075000 < kernel_patch_end_)
            throw std::runtime_error("Startup kernel payload overlaps existing patch");
        startup_kernel_payload_.assign(expected.begin(), expected.end());
    }
    unsigned startup_kernel_word_writes = 0;

    void enableKernelPatch(std::uint32_t start, std::uint32_t size) {
        if (kernel_snapshot_.empty() || kernel_patch_end_ || !size || start % 4 || size % 4
                || start < 0x80000000 || start >= 0x80080000 || size > 0x80080000 - start)
            throw std::runtime_error("Invalid bounded kernel patch range");
        kernel_patch_start_ = start;
        kernel_patch_end_ = start + size;
    }
    std::span<const std::uint8_t> kernelBaseline() const { return kernel_snapshot_; }

    // Single captured startup read, not a timer scheduler or mutable register model.
    void supplyOsdConfig(std::uint32_t word) {
        if (osd_config_supplied) throw std::runtime_error("Duplicate OSD checkpoint");
        osd_config_word = word;
        osd_config_supplied = true;
    }
    void supplyOsdReadback(std::uint32_t expected) {
        if (!osd_config_supplied || osd_readback_supplied || osd_config_reads || expected != osd_config_word
                || ((expected & 0xFFFF1FFF) | 0x2000) != expected)
            throw std::runtime_error("Unsupported OSD same-value readback checkpoint");
        osd_readback_supplied = true;
    }
    unsigned osd_config_reads = 0, osd_config_sets = 0;

    void supplyTimer3Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer3_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 3 capture");
        timer3_mode_ = value;
        timer3_mode_supplied_ = true;
    }

    void supplyTimer2Readback(std::uint32_t expected) {
        if (expected > 0xFFFF || timer2_readback_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 readback validation");
        timer2_readback_expected_ = expected;
        timer2_readback_supplied_ = true;
    }

    void supplyIntcMask(std::uint32_t value) {
        if (value > 0xFFFF || intc_mask_supplied_) throw std::runtime_error("Invalid/repeated INTC mask checkpoint");
        intc_mask = value;
        intc_mask_supplied_ = true;
    }

    void supplyTimer2Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer2_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 capture");
        timer2_mode_ = value;
        timer2_mode_supplied_ = true;
    }

    // Reached CACHE 0x18 path only. This adapter has coherent main RAM and no cache state.
    // Validate a complete aligned line; this does not perform DMA or synchronize an IOP.
    void dataCacheLine(std::uint8_t* rdram, std::uint32_t address) {
        if (address >= 0x20000000 && address < 0x22000000) {
            if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 64);
            address -= 0x20000000;
        }
        if (address & 63) throw UnsupportedGuestMemory(address, 64);
        checkMemory(rdram, address, 64);
        ++data_cache_lines;
    }
    // Owned EE RAM is coherent: no delayed cache writes or valid logical lines.
    // This is the reached indexed two-way scan, not physical R5900 cache emulation.
    void indexedDataCache(std::uint8_t* rdram, R5900Context* ctx,
                          std::uint32_t operation, std::uint32_t address) {
        const auto begin=static_cast<std::uint32_t>(ctx->r[4].low);
        const auto end=static_cast<std::uint32_t>(ctx->r[5].low);
        const auto way=address&1u;
        if ((address&~1u)>0xFC0 || (address&0x3Eu) || begin>end
            || (begin&63u) || (end&63u) || (ctx->cop0_status&0x10000u))
            throw std::runtime_error("Unsupported indexed cache range/status");
        checkMemory(rdram,begin,static_cast<std::size_t>(end)-begin+64u);
        if (operation==0x10u) {
            const auto slot=indexed_cache_tag_reads%128u;
            const auto expected=(slot/2u)*64u+(slot&1u);
            if (indexed_cache_tag_ready || address!=expected
                || (ctx->pc!=(way?0x32F500u:0x32F4C4u) && ctx->pc!=(way?0x32F640u:0x32F604u)))
                throw std::runtime_error("Indexed cache scan order differs");
            indexed_cache_tag_ready=true;
        } else if (operation==0x14u || operation==0x16u) {
            // Invalid coherent tags cannot select a nonzero owned EE range.
            throw std::runtime_error("Unexpected indexed writeback for invalid native tag");
        } else throw std::runtime_error("Unsupported indexed CACHE operation");
        ++indexed_cache_operations;
    }
    std::uint32_t readCacheTagLo(R5900Context* ctx) {
        const auto way=indexed_cache_tag_reads&1u;
        if (!indexed_cache_tag_ready || (ctx->pc!=(way?0x32F508u:0x32F4CCu) && ctx->pc!=(way?0x32F648u:0x32F60Cu)))
            throw std::runtime_error("TagLo read without matching indexed load");
        indexed_cache_tag_ready=false;
        ++indexed_cache_tag_reads;
        return 0; // Explicit invalid tag; verified against both captured indexed ways.
    }
    unsigned indexed_cache_operations=0, indexed_cache_tag_reads=0;
    bool indexed_cache_tag_ready=false;
    unsigned data_cache_lines = 0;
    static constexpr std::size_t sif_iop_size = 2 * 1024 * 1024;
    void supplySifRpcState(std::uint32_t word) {
        if (sif_iop_.empty() || sif_rpc_supplied_ || word != 1)
            throw std::runtime_error("SIF RPC state checkpoint differs or was already supplied");
        sif_rpc_supplied_ = true;
    }
    unsigned sif_rpc_reads = 0;
    void supplySifDma(std::span<const std::uint8_t> before, std::uint32_t result) {
        if (!sif_command_supplied_ || !sif_iop_.empty() || before.size() != sif_iop_size || result != 0x701)
            throw std::runtime_error("SIF DMA checkpoint size/return/order differs");
        std::uint32_t buffer, system_buffer, old_ee;
        std::memcpy(&buffer, before.data() + 0x196C0, 4);
        std::memcpy(&system_buffer, before.data() + 0x196C4, 4);
        std::memcpy(&old_ee, before.data() + 0x196C8, 4);
        if (buffer != 0x19600 || system_buffer != 0x19680 || old_ee != 0x93640)
            throw std::runtime_error("SIF IOP checkpoint table differs");
        sif_iop_.assign(before.begin(), before.end());
        sif_dma_result_ = result;
        sif_ee_receive_address = old_ee;
    }
    void supplySifBind(std::span<const std::uint8_t> bundle) {
        if (!sif_rpc_supplied_ || !sif_bind_iop_.empty() || (bundle.size() != sif_iop_size + 68 && bundle.size() != sif_iop_size + 96))
            throw std::runtime_error("Invalid/repeated RPC bind capture bundle");
        auto word = [&](std::size_t offset) { std::uint32_t value; std::memcpy(&value, bundle.data() + offset, 4); return value; };
        const std::array<std::uint32_t, 16> reply{0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
            0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0};
        if (word(sif_iop_size + 64) != 0x801 || word(0x3F570) != 0x80000592
                || word(0x3F578) != 0x3FB20 || word(0x3F584) != 0
                || word(0x3F5B0) != 0x3F558 || word(0x3F560) != 0x3F570
                || std::memcmp(bundle.data() + sif_iop_size, reply.data(), 64))
            throw std::runtime_error("RPC bind service/queue/response capture differs");
        if (bundle.size() == sif_iop_size + 96) {
            const std::array<std::uint32_t, 7> semaphore{0, 1, 0, 0, 0, 0x3838E8, 3};
            if (std::memcmp(bundle.data() + sif_iop_size + 68, semaphore.data(), 28))
                throw std::runtime_error("RPC call semaphore checkpoint differs");
            sif_call_sema_supplied_ = true;
        }
        sif_bind_iop_.assign(bundle.begin(), bundle.begin() + sif_iop_size);
        std::copy_n(bundle.data() + sif_iop_size, 64, sif_bind_reply_.data());
    }
    void supplyCdInit(std::span<const std::uint8_t> bundle) {
        if (sif_bind_iop_.empty() || !sif_call_sema_supplied_ || cd_init_supplied_ || (bundle.size() != 88 && bundle.size() != 128))
            throw std::runtime_error("Invalid/repeated CD-init completion bundle");
        const std::array<std::uint32_t, 22> expected{0, 0x902,
            0x1000, 0x3695C0, 0x80000008, 0, 0, 0, 0, 0x3A87A8, 0x8000000A, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 0, 0};
        if (std::memcmp(bundle.data(), expected.data(), 88))
            throw std::runtime_error("CD-init captured mode/result/reply differs");
        if (bundle.size() == 128) {
            const std::array<std::uint32_t, 10> expected_semaphores{0x370000, 1, 0, 0, 0x370000, 0x3825E8, 3, 4, 5, 6};
            if (std::memcmp(bundle.data() + 88, expected_semaphores.data(), 40))
                throw std::runtime_error("CD semaphore setup checkpoint differs");
            cd_semaphores_supplied_ = true;
        }
        std::copy_n(bundle.begin(), 88, cd_init_bundle_.begin());
        cd_init_supplied_ = true;
    }
    void supplyScmdBind(std::span<const std::uint8_t> bundle) {
        const std::array<std::uint32_t, 22> expected{0x370000,1,0,0,2,0x3838D8,
            0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8,0,0,0,0,0};
        if (!cd_semaphores_supplied_ || scmd_bind_supplied_ || (bundle.size() != 88 && bundle.size() != 184 && bundle.size() != 188 && bundle.size() != 192 && bundle.size() != 204 && bundle.size() != 224 && bundle.size() != 228 && bundle.size() != 236 && bundle.size() != 244 && bundle.size() != 252 && bundle.size() != 316 && bundle.size() != 324 && bundle.size() != 616 && bundle.size() != 988 && bundle.size() != 1124 && bundle.size() != 1332 && bundle.size() != 1464 && bundle.size() != 1596)
                || std::memcmp(bundle.data(), expected.data(), 88))
            throw std::runtime_error("Invalid SCMD bind checkpoint");
        if (bundle.size() >= 184) {
            const std::array<std::uint32_t,24> mmode{0,1,0,0,0x370000,0x3838E8,
                0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0,2,0};
            if(std::memcmp(bundle.data()+88,mmode.data(),96)) throw std::runtime_error("Invalid media-mode checkpoint");
            std::copy_n(bundle.data()+112,64,scmd_mmode_reply_.data());scmd_mmode_supplied_=true;
        }
        if (bundle.size() >= 188) {
            std::uint32_t stop_word; std::memcpy(&stop_word,bundle.data()+184,4);
            if (stop_word) throw std::runtime_error("SIF stop checkpoint differs");
            sif_stop_supplied_=true;
        }
        if (bundle.size() >= 192) {
            std::uint32_t subaddr; std::memcpy(&subaddr,bundle.data()+188,4);
            if (subaddr!=0x19600) throw std::runtime_error("Reset SUBADDR checkpoint differs");
            sif_reset_subaddr_supplied_=true;
        }
        if (bundle.size() >= 204) {
            const std::array<std::uint32_t,3> flags{0x30000,0x30000,0x30000};
            if (std::memcmp(bundle.data()+192,flags.data(),12)) throw std::runtime_error("Reset SMFLAG checkpoint differs");
            sif_smflag=0x30000; reset_smflag_supplied_=true;
        }
        if(bundle.size()>=224) {
            const std::array<std::uint32_t,5> completion{0xE01,0x20000,0,0,0};
            if(std::memcmp(bundle.data()+204,completion.data(),20))throw std::runtime_error("Invalid reboot submission/clear checkpoints");
            reset_submission_supplied_=true;
        }
        if(bundle.size()>=228) {
            std::uint32_t bit;std::memcpy(&bit,bundle.data()+224,4);
            if(bit!=0x40000)throw std::runtime_error("Invalid ready-bit checkpoint");
            reset_ready_supplied_=true;
        }
        if(bundle.size()>=236) {
            std::uint64_t csr;std::memcpy(&csr,bundle.data()+228,8);
            if(csr!=0x551B4000)throw std::runtime_error("Invalid GS CSR checkpoint");
            gs_csr_supplied_=true;
        }
        if(bundle.size()>=244) {
            std::uint64_t result;std::memcpy(&result,bundle.data()+236,8);
            if(result!=0xFF00)throw std::runtime_error("Invalid GS IMR checkpoint");
            gs_imr_supplied_=true;
        }
        if(bundle.size()>=252) {
            std::uint64_t result;std::memcpy(&result,bundle.data()+244,8);
            if(result)throw std::runtime_error("Invalid GS CRT checkpoint");
            gs_crt_supplied_=true;
        }
        if(bundle.size()>=316) {
            const std::array<std::uint32_t,16> evidence{12,0x1802,0x1FFEAD0,1,8,0x1802,0x1FFEAD0,1,
                12,0x1802,0x1FFEAD0,4,8,0x1802,0x1FFEAD0,1};
            if(std::memcmp(bundle.data()+252,evidence.data(),64))throw std::runtime_error("INTC evidence differs");
            gs_interrupt_supplied_=true;
        }
        if(bundle.size()>=324) {
            std::uint64_t field;std::memcpy(&field,bundle.data()+316,8);
            if(field!=0x551B6004)throw std::runtime_error("GS field checkpoint differs");
            gs_field_supplied_=true;
        }
        if(bundle.size()>=616) {
            const std::array<std::uint32_t,9> idle{0,0,0x1FFEAF0,0,0,0,1,0x800040,0x1201};
            const std::array<std::uint64_t,2> results{15,0};
            if(std::memcmp(bundle.data()+324,idle.data(),36) || std::memcmp(bundle.data()+360,results.data(),16))
                throw std::runtime_error("GIF idle/staging/reference return evidence differs");
            gif_dma.supply(bundle.subspan(376,240));gif_dma_supplied_=true;
            if(bundle.size()>=988)gif_dma.supplySync(bundle.subspan(616,372));
            if(bundle.size()>=1124)post_sif.supply(bundle.subspan(988,136));
            if(bundle.size()>=1332)post_cd.supply(bundle.subspan(1124,bundle.size()>=1464?340:208));
            if(bundle.size()>=1596)post_loadfile.supply(bundle.subspan(1464));
        }
        std::copy_n(bundle.data()+24,64,scmd_reply_.data()); scmd_bind_supplied_=true;
    }
    bool iop_reset_pending=false, prior_iop_services_valid=true;
    GifDmaStaging gif_dma;
    PostResetSif post_sif;
    PostResetCd post_cd;
    PostResetLoadfile post_loadfile;
    unsigned native_module_loads=0;
    std::string native_module_request,native_iop_error;
    unsigned native_iop_steps=0,native_iop_targets=0,native_iop_waiters=0;
    std::uint32_t native_iop_pc=0;
#ifdef BT2_NATIVE_IOP
    std::shared_ptr<NativeIopService> native_iop;
#endif
    unsigned heap_binds=0,heap_calls=0,heap_frees=0;
    bool sound_thread_start_supplied=false,sound_thread_checkpoint=false;
    unsigned sound_thread_queries=0,sound_thread_starts=0;
    void supplySoundThreadStart(std::span<const std::uint8_t> evidence) {
        const std::array<std::uint32_t,14> expected={0x70030C11,3,0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0};
        if(sound_thread_start_supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("EE sound thread-start evidence differs/repeated");
        sound_thread_start_supplied=true;
    }
    unsigned mc_rpc_binds=0,mc_rpc_calls=0,mc_lock_waits=0,mc_lock_signals=0,mc_lock_polls=0;
    unsigned sound_rpc_binds=0,sound_rpc_calls=0,sounds_rpc_binds=0,sounds_rpc_calls=0,dbc_rpc_binds=0,dbc_rpc_calls=0,dbc_lock_waits=0,dbc_lock_signals=0;
    std::vector<std::string> ee_console_messages;
    unsigned dmac_startup_writes=0;
    std::uint32_t dmac_control=0,dmac_priority=0;
    VifStartup vif_startup;
    TextureGifStartup texture_gif;
    unsigned ee_heap_lock_waits=0,ee_heap_lock_signals=0,heap_end_queries=0;
    unsigned timer1_setup_writes=0;
    std::uint32_t timer1_mode=0;
    // GS privileged display registers (0x12000000 + 16*n) as last written; PMODE n=0 .. BGCOLOR n=14.
    std::array<std::uint64_t,15> gs_privileged{};unsigned gs_display_writes=0,gs_display_flips=0;
    // Frame-phase GIF DMA (channel 2), normal mode from main RAM; transfers complete synchronously
    // into the general GS path (FRAME_GIF_PLAN.md).
    struct FrameGif {std::uint32_t chcr=0,madr=0,qwc=0;unsigned starts=0;std::uint64_t bytes=0;} frame_gif;unsigned frame_syncs=0;
    // Frame-phase VIF1 DMA (channel 1), chain mode with tag transfer; tags are walked synchronously
    // and their VIF codes interpreted. Only the reached codes are supported (FRAME_GIF_PLAN.md).
    struct FrameVif1 {std::uint32_t chcr=0,tadr=0,qwc=0;bool cis=false;unsigned starts=0,tags=0,codes=0,directs=0;std::uint64_t direct_qwords=0;
        std::uint32_t blocked_code=0,blocked_tag=0,blocked_tadr=0;unsigned unsupported=0;} frame_vif1;
    struct FrameVif1Unsupported {std::uint32_t code,tag;};
    void frameVif1Start(std::uint32_t value) {
        auto& v=frame_vif1;
        if(value!=0x145 || v.qwc)throw std::runtime_error("Unsupported VIF1 DMA start");
        // Walk the whole chain first. Each DMA tag contributes its upper 64 bits (two VIF codes at
        // qword positions 2-3) and then its data qwords; codes are parsed with their real lengths.
        // NOP and FLUSH/FLUSHE/FLUSHA (no VU1 program or other path is running) have no effect;
        // DIRECT/DIRECTHL hand quadword-aligned PATH2 data to the GS. Any other code, or a DIRECT
        // payload that crosses a tag, stops at the channel start before any effect.
        struct Word {std::uint32_t value,tag;unsigned segment,phase;const std::uint8_t* at;};
        std::vector<Word> stream;std::uint32_t tag=v.tadr,next_tadr=0;unsigned tags=0,segment=0;
        for(unsigned n=0;;++n) {
            if(n>=65536 || tag%16 || tag>=ram_size-16)throw std::runtime_error("VIF1 DMA chain outside RAM");
            std::uint64_t lo;std::memcpy(&lo,memory_.data()+tag,8);
            const std::uint32_t qwc=std::uint32_t(lo&0xFFFF),id=std::uint32_t((lo>>28)&7),addr=std::uint32_t((lo>>32)&0x7FFFFFF0);
            if(lo&0x80000000)throw std::runtime_error("VIF1 DMA tag IRQ is not modelled");
            if(id==5 || id==6)throw std::runtime_error("VIF1 DMA call/ret tags are not modelled");
            ++segment;for(unsigned k=2;k<4;++k){std::uint32_t w;std::memcpy(&w,memory_.data()+tag+k*4,4);stream.push_back({w,tag,segment,k,memory_.data()+tag+k*4});}
            ++tags;
            if(qwc) {
                const auto data=(id==1 || id==2 || id==7)?tag+16:addr;
                if(data>=ram_size || std::size_t(qwc)*16>ram_size-data)throw std::runtime_error("VIF1 DMA data outside RAM");
                ++segment;
                for(std::uint32_t k=0;k<qwc*4;++k){std::uint32_t w;std::memcpy(&w,memory_.data()+data+k*4,4);stream.push_back({w,tag,segment,k%4,memory_.data()+data+k*4});}
            }
            if(id==0 || id==7) {next_tadr=id==7?tag+16+qwc*16:tag+16;break;}
            tag=id==1?tag+16+qwc*16:id==2?addr:tag+16;
        }
        std::vector<std::span<const std::uint8_t>> directs;std::uint64_t direct_qwords=0;
        try {
            for(std::size_t i=0;i<stream.size();) {
                const auto& w=stream[i];const unsigned cmd=(w.value>>24)&0x7F;
                if(cmd==0 || cmd==0x10 || cmd==0x11 || cmd==0x13) {++i;continue;}
                if(cmd==0x50 || cmd==0x51) {
                    const std::size_t qwords=(w.value&0xFFFF)?(w.value&0xFFFF):65536,first=i+1,last=first+qwords*4;
                    if(last>stream.size() || stream[first].phase || stream[first].segment!=stream[last-1].segment)
                        throw FrameVif1Unsupported{w.value,w.tag};
                    directs.emplace_back(stream[first].at,qwords*16);direct_qwords+=qwords;i=last;continue;
                }
                throw FrameVif1Unsupported{w.value,w.tag};
            }
            gif_dma.gs.transferAll(directs); // One GS transaction: refused as a whole, without effects.
        } catch(const FrameVif1Unsupported& u) {
            v.blocked_code=u.code;v.blocked_tag=u.tag;v.blocked_tadr=v.tadr;++v.unsupported;
            throw UnsupportedGuestMemory(0x10009000,4); // Checked boundary at the channel start.
        } catch(const std::runtime_error& e) {
            std::cerr<<"VIF1 DIRECT refused by the GS model: "<<e.what()<<"\n";
            v.blocked_code=0x50000000;v.blocked_tag=v.tadr;v.blocked_tadr=v.tadr;++v.unsupported;
            throw UnsupportedGuestMemory(0x10009000,4);
        }
        v.tadr=next_tadr;v.tags+=tags;v.codes+=unsigned(stream.size());v.directs+=unsigned(directs.size());v.direct_qwords+=direct_qwords;
        v.chcr=value&~0x100u;v.cis=true;++v.starts;
    }
    bool frameGifContext() const {return gs_display_flips>=1 && current_thread_id==1 && !in_interrupt_;}
    LogicalTimer1 timer1;unsigned timer1_starts=0,timer1_polls=0,timer1_wait_polls=0,timer1_stops=0,timer1_vblanks=0,sound_tick_boundaries=0;
    unsigned frame_limit=0;bool frame_budget_reached=false; // Optional probe frame budget (display flips).
    std::function<void()> on_display_flip; // Host presentation hook; never changes guest state.
    unsigned graph_startup_phase=0;
    // Main's display re-initialization after the first archive load. The startup clear routine
    // (0x100624 -> 0x100258) runs again: 0 CSR reset, 1 CSR read, 2 IMR, 3 VSync handler removed,
    // 4 CRT(1,2,1), 5 VSync wait, 6 two GIF requests. 7 field sample (0x271178). Display setup
    // (0x100650 -> 0x271078): 8 CSR reset, 9 CSR read, 10 IMR, 11 CRT(1,2,0), 12 handler re-added.
    bool sounds_hardware_timers_supported=true; // IOP_HARDWARE_TIMER_PLAN.md
    bool sound_voice_transfers_supported=true; // Owned SPU2 voice transfers (IOP_SPU2_TRANSFER_PLAN.md).
    unsigned fileio_binds=0,fileio_calls=0,fileio_completions=0;
    bool fileio_workers_supported=true;
    // The SOUNDS hardware timer's worker reads SPU2 voice state (ENDX) on its ticks; until that is
    // modelled (SPU2_VOICE_PLAN.md), a VBlank that would deliver timer ticks is a checked boundary.
    bool sounds_timer_ticks_supported=false;
    unsigned display_reset_phase=0,polled_vblanks=0,timer2_deliveries=0,file_load_idles=0,iop_uploads=0,iop_upload_polls=0;
    unsigned delivering_cause_=0;
    LogicalTimer2 timer2;
    bool timer2Context(const R5900Context* context) const {
        // Main after admission, or the cause-11 handler during a logical delivery.
        return timer2.started() && ((in_interrupt_ && delivering_cause_==11)
            || (!in_interrupt_ && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active)) && context;
    }
    std::uint32_t vsync_flag=0,vsync_csr=0;unsigned frame_vsync_deliveries=0;
    std::uint32_t display_intc_stat=0;
    bool display_field=true; // Toggles per polled VBlank; anchored to the original 0x271178 sample (field 1).
    std::optional<GifDmaStaging> clear_repeat_gif;
    bool displayResetStep(const R5900Context* context,unsigned phase,std::uint32_t pc,std::uint32_t frame) const {
        return display_reset_phase==phase && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active
            && !in_interrupt_ && graph_startup_phase==4 && gs_csr_reset_writes==1 && cri_rpc_calls==42
            && context->pc==pc && context->r[29].low==initial_sp-frame;
    }
    // Main's second embedded-buffer load (0x100634 -> 0x320580), between the field sample and display setup.
    bool embeddedReload() const {
        return display_reset_phase==8 && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active
            && !in_interrupt_ && cri_rpc_calls==42 && iop_epoch==1;
    }
    bool displayVsyncWait(const R5900Context* context,std::uint32_t pc,std::uint32_t offset) const {
        // SyncV without a callback polls INTC_STAT directly while cause 2 is disabled.
        return !(intc_mask&4) && (displayResetStep(context,5,pc,656-offset) || displayResetStep(context,7,pc,96-offset));
    }
    std::uint64_t graph_halfword_checkpoint=0;
    bool heap_allocation_supplied=false,heap_copy_supplied=false;
    unsigned heap_transfers=0,heap_dma_polls=0;
    void supplyHeapAllocation(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,17> expected={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0,0xCE500};
        if(heap_allocation_supplied || (evidence.size()!=sizeof(expected) && evidence.size()!=sizeof(expected)+20) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("IOP heap allocation evidence differs or repeated");
        if(evidence.size()==sizeof(expected)+20) {
            constexpr std::array<std::uint32_t,5> copy={0x369F90,0xCE500,0x360,0,0xFFFFFFFF};
            if(std::memcmp(evidence.data()+sizeof(expected),copy.data(),sizeof(copy)))throw std::runtime_error("Heap copy evidence differs");
            heap_copy_supplied=true;
        }
        heap_allocation_supplied=true;
    }
    bool heap_bind_supplied=false;
    void supplyHeapBind(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,16> expected={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0};
        if(heap_bind_supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("IOP heap bind evidence differs or repeated");
        heap_bind_supplied=true;
    }
    unsigned serviceOperations() const {return cri_rpc_calls+cri_rpc_binds+ncmd_ready_calls+ncmd_rpc_binds+disc_rpc_calls+disc_rpc_binds+mc_rpc_binds+mc_rpc_calls+ post_cd.operations()+post_loadfile.operations()+native_module_loads+heap_binds+heap_calls+heap_frees+sound_rpc_binds+sound_rpc_calls+sounds_rpc_binds+sounds_rpc_calls+dbc_rpc_binds+dbc_rpc_calls;}
    IntcCheckpoint gs_interrupt;
    unsigned gs_field_reads=0;
    unsigned gs_csr_reset_writes=0,gs_csr_reads=0,gs_imr_writes=0,gs_crt_calls=0;
    std::uint32_t gs_imr=0;
    std::array<std::uint32_t,3> gs_crt_request{};
    unsigned iop_epoch=0, reset_clear_writes=0, reset_ready_checkpoint_reads=0;
    std::uint32_t reset_rpcinit=1, reset_subaddr=0x19600;
    unsigned sif_stop_calls = 0, sif_reset_subaddr_reads = 0, reset_smflag_writes = 0;
    std::uint32_t sif_smflag = 0;
    std::uint32_t sif2_chcr = 0, sif2_qwc = 0;
    unsigned scmd_bind_calls = 0, scmd_mmode_calls = 0;
    unsigned semaphore_polls = 0;
    unsigned cd_init_calls = 0, cd_init_context_preservations = 0, cd_semaphores_created = 0;
    unsigned sif_bind_callbacks = 0;
    unsigned sif_bind_context_preservations = 0;
    unsigned semaphore_deletions = 0;
    unsigned rpc_call_semaphores = 0;
    std::uint32_t rpc_call_sema_attributes = 0;
    std::span<const std::uint8_t> sifIopMemory() const { return sif_iop_; }
    unsigned sif_dma_submissions = 0;
    std::uint32_t sif_ee_receive_address = 0;
    void supplySifCommand(std::uint32_t word) {
        if (!dma5_control_supplied_ || sif_command_supplied_ || word != 0x19600)
            throw std::runtime_error("SIF checkpoint differs or was already supplied");
        sif_command_word_ = word;
        sif_command_supplied_ = true;
    }
    unsigned sif_command_reads = 0;
    void supplyDmaStatus(std::uint32_t word) {
        if (dma_status_supplied_ || word != 0x00800040)
            throw std::runtime_error("Unsupported/repeated DMA status checkpoint");
        dma_status_word_ = word;
        dmac_mask = word & 0xFFFF0000;
        dma_status_supplied_ = true;
    }
    void supplyDma5Control(std::uint32_t word) {
        if (!dma_status_supplied_ || dma5_control_supplied_ || word != 0x184)
            throw std::runtime_error("Unsupported/repeated DMA 5 control checkpoint");
        dma5_control_word_ = word;
        dma5_control_supplied_ = true;
    }
    unsigned dma_status_reads = 0, dma5_control_reads = 0;
    struct DmacHandler { std::uint32_t channel, address, arg, gp; };
    std::vector<DmacHandler> dmac_handlers;
    std::vector<std::uint32_t> dmac_handler_order;
    std::uint32_t dmac_mask = 0, dmac_enable_calls = 0, dmac_disable_calls = 0, dmac_removals = 0;

    // Emitted slow-path memory ABI; unsupported MMIO still fails explicitly.
    void Store128(std::uint8_t* rdram,R5900Context* context,std::uint32_t address,const ProbeGpr& value) {
        if(address==0x10005000 && context->r[29].low==initial_sp-32
            && ((vif_startup.phase==3 && context->pc==0x30FFB4) || (vif_startup.phase==4 && context->pc==0x30FFBC))) {
            const auto expected=read128(rdram,0x3680A0+(vif_startup.phase-3)*16);
            if(value.low!=expected.low || value.high!=expected.high)throw UnsupportedGuestMemory(address,16);
            std::array<std::uint32_t,4> words;std::memcpy(words.data(),&value,16);
            vif_startup.consume(words);++vif_startup.phase;return;
        }
        write128(rdram,address,value); // Other FIFO/MMIO paths still reject through checked RAM bounds.
    }
    void writeVuFbrst(R5900Context* context,std::uint32_t value) {
        if(context->pc!=0x30FF90 || context->r[29].low!=initial_sp-32 || vif_startup.phase!=2 || value!=0x200)
            throw std::runtime_error("Unsupported VU control write");
        ++vif_startup.vu_resets;vif_startup.phase=3;context->vu0_fbrst=value&0xC0C;
    }
    void handleBreak(std::uint8_t*,R5900Context*) {
        throw std::runtime_error("Unsupported guest BREAK instruction");
    }
    void Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
        checkMemory(rdram,0,0);
        if(address==0x12001000 && vif_startup.phase==6 && !graph_startup_phase && value==0x200
            && context->pc==0x30FE44 && context->r[29].low==initial_sp-112 && gs_csr_reset_writes==1) {
            gif_dma.gs.resetDrawingState();graph_startup_phase=1;return;
        }
        if(address==0x12001000 && value==0x200 && (displayResetStep(context,0,0x30FE44,688) || displayResetStep(context,8,0x30FE44,128))) {
            gif_dma.gs.resetDrawingState();++display_reset_phase;return;
        }
        // Privileged GS display registers after display setup (main's thread): PMODE, SMODE2, DISPFB1/2,
        // DISPLAY1/2, EXTWRITE (off only) and BGCOLOR are recorded as display state for presentation.
        // CRT timing registers stay with SetGsCrt. The library flip (0x311860) is counted at its last
        // store. No presentation is claimed.
        if(current_thread_id==1 && !in_interrupt_ && display_reset_phase==13 && address>=0x12000000 && address<=0x120000E0 && !(address&15)) {
            const auto n=(address-0x12000000)/16;
            if(n!=0 && n!=2 && n!=7 && n!=8 && n!=9 && n!=10 && n!=13 && n!=14)throw UnsupportedGuestMemory(address,8);
            if(n==13 && value)throw UnsupportedGuestMemory(address,8); // EXTWRITE: write-back circuit stays off.
            if(!gs_display_flips && context->pc!=0x3118D0 && !gs_display_writes)throw UnsupportedGuestMemory(address,8); // First write is the library flip.
            gs_privileged[n]=value;++gs_display_writes;
            if(context->pc==0x311908 && n==14) {
                ++gs_display_flips;
                if(on_display_flip)on_display_flip(); // Host presentation of the completed flip (viewer mode).
                // Probe frame budget: stop after this completed flip (its store is applied) as a checked boundary.
                if(frame_limit && gs_display_flips==frame_limit) {frame_budget_reached=true;throw UnsupportedGuestMemory(address,8);}
            }
            return;
        }
        if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes || gs_csr_reads
                || context->pc!=0x30FE44 || context->r[29].low!=initial_sp-688 || value!=0x200)
            throw UnsupportedGuestMemory(address,8);
        // Reached reset accepted only with matching reference readback evidence.
        // Reset only the supported drawing-register defaults; VRAM stays unknown.
        gif_dma.gs.resetDrawingState();
        ++gs_csr_reset_writes;
    }
    std::uint64_t Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        checkMemory(rdram,0,0);
        if(address==0x12001000 && graph_startup_phase==1 && context->pc==0x30FE50 && context->r[29].low==initial_sp-112) {
            graph_startup_phase=2;return 0x551B4000; // Independently captured second original CSR load; no native video clock.
        }
        if(address==0x12001000 && (displayResetStep(context,1,0x30FE50,688) || displayResetStep(context,9,0x30FE50,128))) {
            // Only the GS revision byte (bits 16..23) is consumed; the original keeps 0x1B.
            ++display_reset_phase;return 0x551B4000;
        }
        if(address==0x12001000 && (displayResetStep(context,5,0x3104F4,640) || displayResetStep(context,7,0x3104F4,80))) {
            // Only FIELD (bit 13) is consumed, after this wait's polled VBlank.
            if(polled_vblanks!=(display_reset_phase==5?1u:2u) || (display_intc_stat&4))throw UnsupportedGuestMemory(address,8); // One acknowledged VBlank per wait.
            ++display_reset_phase;if(display_reset_phase==6)clear_repeat_gif=gif_dma.repeat();
            return 0x551B4004|(display_field?0x2000u:0u);
        }
        if(address==0x12001000 && gs_field_supplied_ && gs_interrupt.acknowledgements==2 && !gs_field_reads
            && context->pc==0x3104F4 && context->r[29].low==initial_sp-640) {
            ++gs_field_reads;return 0x551B6004; // Independent captured field, not timed native GS state.
        }
        if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes!=1 || gs_csr_reads
                || context->pc!=0x30FE50 || context->r[29].low!=initial_sp-688)
            throw UnsupportedGuestMemory(address,8);
        ++gs_csr_reads;return 0x551B4000; // Explicit captured reference readback, not native GS state.
    }
    void Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {
        if(address==0x10000810 && context->pc==0x28C770 && context->r[29].low==initial_sp-112
            && texture_gif.phase==8 && !timer1_setup_writes && value==3) {
            checkMemory(rdram,0,0);timer1_mode=value;timer1.configure(value);++timer1_setup_writes;return; // Disabled Timer 1 configuration only.
        }
        if(frameGifContext() && (address==0x10009000 || address==0x10009020 || address==0x10009030 || (address==0x1000E010 && value==2))) {
            checkMemory(rdram,0,0);auto& v=frame_vif1;
            if(address==0x1000E010) {v.cis=false;return;} // W1C of CIS1 only.
            if(v.chcr&0x100)throw UnsupportedGuestMemory(address,4);
            if(address==0x10009020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);v.qwc=value;return;}
            if(address==0x10009030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.tadr=value&0x0FFFFFFF;return;}
            frameVif1Start(value);return;
        }
        if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020)) {
            checkMemory(rdram,0,0);auto& g=frame_gif;
            if(g.chcr&0x100)throw UnsupportedGuestMemory(address,4); // Writes while a transfer runs.
            if(address==0x1000A020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);g.qwc=value;return;}
            if(address==0x1000A010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.madr=value&0x0FFFFFFF;return;}
            if(value!=0x101 || !g.qwc || g.madr>=ram_size || std::size_t(g.qwc)*16>ram_size-g.madr)throw UnsupportedGuestMemory(address,4);
            gif_dma.gs.transfer(std::span<const std::uint8_t>(memory_.data()+g.madr,std::size_t(g.qwc)*16));
            g.bytes+=std::size_t(g.qwc)*16;g.madr+=g.qwc*16;g.qwc=0;g.chcr=value&~0x100u;++g.starts;return;
        }
        // Main's H-BLANK delay helpers (0x28C778 start, 0x28C790 wait >= 400 and stop): logical Timer 1.
        if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
            if(address==0x10000810 && context->pc==0x28C78C && value==(timer1.mode()|0x80)) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_starts;timer1_wait_polls=0;return;}
            if(address==0x10000810 && context->pc==0x28C7CC && value==(timer1.mode()&~0x80u) && timer1.counting()) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_stops;return;}
            if(address==0x10000800 && context->pc==0x28C7D4 && !value && !timer1.counting()) {checkMemory(rdram,0,0);timer1.writeCount(0);return;}
        }
        if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
            if(address==0x1000A020 && context->pc==0x28B96C) {checkMemory(rdram,0,0);texture_gif.count(value);return;}
            if(address==0x1000A010 && context->pc==0x28B978) {checkMemory(rdram,0,0);texture_gif.address(value,initial_sp-112,memory());return;}
            if(address==0x1000A000 && context->pc==0x28B98C) {checkMemory(rdram,0,0);texture_gif.start(value,memory(),gif_dma.gs);return;}
        }
        if(dmac_startup_writes==3 && heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
            if(address==0x10003C10 && context->pc==0x30FF70 && !vif_startup.phase && value==1) {
                checkMemory(rdram,0,0);vif_startup.phase=1;return;
            }
            if(address==0x10003C20 && context->pc==0x30FF80 && vif_startup.phase==1 && value==2) {
                checkMemory(rdram,0,0);vif_startup.error=value;vif_startup.phase=2;return;
            }
        }
        if(heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
            if(address==0x1000E000 && context->pc==0x28B2B4 && !dmac_startup_writes && value==1) {
                checkMemory(rdram,0,0);dmac_control=value;++dmac_startup_writes;return;
            }
            if(address==0x1000E020 && context->pc==0x28B2B8 && dmac_startup_writes==1 && value==0x83FF0002) {
                checkMemory(rdram,0,0);dmac_priority=value;++dmac_startup_writes;return;
            }
        }
        if((address==0xB0001010 || address==0xB0001020) && context->pc==0x338B30 && timer2Context(context)) {
            checkMemory(rdram,0,0);if(address==0xB0001010)timer2.writeMode(value);else timer2.writeComp(value);return;
        }
        if(address==0x1000F000 && value==4 && (displayVsyncWait(context,0x32F10C,0) || displayVsyncWait(context,0x32F160,0) || (frameGifContext() && timer1_stops && (context->pc==0x32F1AC || context->pc==0x32F1FC)))) {
            checkMemory(rdram,0,0);display_intc_stat&=~value;return; // Write-one-to-clear VBLANK_START.
        }
        if(clear_repeat_gif && display_reset_phase==6 && context->r[29].low==initial_sp-624 && displayResetStep(context,6,context->pc,624)) {
            if(address==0x1000A000 && context->pc==0x3101A8) {checkMemory(rdram,0,0);clear_repeat_gif->start(value,memory(),gif_dma.gs);return;}
            if(address==0x1000A020 && context->pc==0x310148) {checkMemory(rdram,0,0);clear_repeat_gif->setCount(value);return;}
            if(address==0x1000A010 && context->pc==0x310194) {
                checkMemory(rdram,0,0);clear_repeat_gif->setAddress(value,initial_sp-(clear_repeat_gif->starts?288u:528u),memory());return;
            }
        }
        if(address==0x1000F000 && gs_interrupt_supplied_ && gs_crt_calls==1
            && context->r[29].low==initial_sp-656 && value==4
            && ((context->pc==0x32F10C && !gs_interrupt.acknowledgements)
                || (context->pc==0x32F160 && gs_interrupt.acknowledgements==1))) {
            checkMemory(rdram,0,0);
            if(!gs_interrupt.acknowledgements) {
                if(intc_mask!=0x1802)throw std::runtime_error("INTC mask differs from checkpoint");
                gs_interrupt.arm(12,intc_mask);
            }
            gs_interrupt.acknowledge(value);return;
        }
        if(gif_dma_supplied_ && gs_field_reads==1 && context->r[29].low==initial_sp-624) {
            if(address==0x1000A000 && context->pc==0x3101A8) {
                checkMemory(rdram,0,0);gif_dma.start(value,memory());return;
            }
            if(address==0x1000A020 && context->pc==0x310148) {
                checkMemory(rdram,0,0);gif_dma.setCount(value);return;
            }
            if(address==0x1000A010 && context->pc==0x310194) {
                checkMemory(rdram,0,0);gif_dma.setAddress(value,initial_sp-(gif_dma.starts?288u:528u),memory());return;
            }
        }
        write32(rdram, address, value);
    }

    std::uint32_t Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020)) {
            checkMemory(rdram,0,0);if(context->pc==0x3105A4)++frame_syncs;
            return address==0x1000A000?frame_gif.chcr:address==0x1000A010?frame_gif.madr:frame_gif.qwc;
        }
        // Frame phase: GIF and VIF1 transfers complete synchronously and no VU1 work is started, so
        // channel and path status reads see idle (VIF1_STAT 0x40 and GIF_STAT 0 as captured at the
        // startup path sync).
        if(frameGifContext() && !(frame_gif.chcr&0x100) && !(frame_vif1.chcr&0x100)) {
            if(address==0x10009000 || address==0x10009020 || address==0x10009030) {
                checkMemory(rdram,0,0);return address==0x10009000?frame_vif1.chcr:address==0x10009020?frame_vif1.qwc:frame_vif1.tadr;
            }
            if(address==0x10003C00) {checkMemory(rdram,0,0);return 0x40;}
            if(address==0x10003020) {checkMemory(rdram,0,0);return 0;}
        }
        if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
            if(address==0x10000810 && (context->pc==0x28C780 || context->pc==0x28C7C0)) {checkMemory(rdram,0,0);return timer1.mode();}
            if(address==0x10000800 && context->pc==0x28C798 && timer1.counting()) {
                // Logical time: each poll of the delay loop waits one line; a crossed field boundary
                // delivers that VBLANK_START first (interrupt handlers and woken threads run).
                checkMemory(rdram,0,0);
                ++timer1_polls;if(++timer1_wait_polls>100000)throw UnsupportedGuestMemory(address,4); // One wait never ends.
                const auto target=timer1.nextLine();
                if(timer1.crossesField(target)) {deliverVblank(rdram,context);++timer1_vblanks;}
                timer1.lineTo(target);return timer1.count();
            }
        }
        if((address==0x10001000 || address==0x10001010) && timer2Context(context)) {
            checkMemory(rdram,0,0);return address==0x10001000?timer2.count():timer2.mode();
        }
        if(address==0x1000F000 && frameGifContext() && timer1_stops && context->pc==0x32F1D0 && vsync_flag) {
            checkMemory(rdram,0,0);deliverVblank(rdram,context);return display_intc_stat;
        }
        if(address==0x1000F000 && displayVsyncWait(context,0x32F130,0)) {
            checkMemory(rdram,0,0);
            // Logical time: a poll that finds VBLANK_START clear advances one VBlank (START and END latch).
            // Cause 2 is disabled here, so no handler runs; FIELD toggles in interlaced mode.
            if(!(display_intc_stat&4)) {display_intc_stat|=0xC;display_field=!display_field;++polled_vblanks;advanceLogicalField();}
            return display_intc_stat;
        }
        if(clear_repeat_gif && displayResetStep(context,6,context->pc,624)) {
            auto& g=*clear_repeat_gif;
            if(address==0x1000A000 && context->pc==0x3100E4) {checkMemory(rdram,0,0);return g.readIdle();}
            if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return g.readVifIdle();}
            if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {checkMemory(rdram,0,0);return g.readStarted(context->pc==0x3105CC);}
            if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return g.readVifStatus();}
            if(address==0x10003020 && context->pc==0x310664) {
                checkMemory(rdram,0,0);const auto status=g.readGifStatus();
                if(g.starts==2)display_reset_phase=7; // Both clear requests completed.
                return status;
            }
        }
        if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
            if(address==0x1000A000 && context->pc==0x28B928) {checkMemory(rdram,0,0);return texture_gif.idle();}
            if(address==0x10003020 && context->pc==0x28B944) {checkMemory(rdram,0,0);return texture_gif.pathIdle();}
            if(address==0x1000A000 && context->pc==0x28B998) {checkMemory(rdram,0,0);return texture_gif.poll();}
            if(address==0x10003020 && context->pc==0x28B9B4) {checkMemory(rdram,0,0);return texture_gif.pathComplete();}
        }
        if(post_sif.supplied && gif_dma.completions==2 && iop_epoch==1 && context->r[29].low==initial_sp-192) {
            if(address==0x1000E010 && context->pc==0x3319CC) {checkMemory(rdram,0,0);return post_sif.status();}
            if(address==0x1000C000 && context->pc==0x3319EC) {checkMemory(rdram,0,0);return post_sif.channel();}
        }
        if(gif_dma.syncSupplied() && gif_dma.starts>=1 && gif_dma.starts<=2 && context->r[29].low==initial_sp-624) {
            if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return gif_dma.readVifIdle();}
            if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {
                checkMemory(rdram,0,0);return gif_dma.readStarted(context->pc==0x3105CC);
            }
            if(address==0x10003020 && context->pc==0x310664) {checkMemory(rdram,0,0);return gif_dma.readGifStatus();}
            if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return gif_dma.readVifStatus();}
        }
        if(address==0x1000A000 && gif_dma_supplied_ && gs_field_reads==1 && context->pc==0x3100E4
            && context->r[29].low==initial_sp-624) {
            checkMemory(rdram,0,0);return gif_dma.readIdle();
        }
        if(address==0x1000F000 && gs_interrupt_supplied_ && context->pc==0x32F130
            && context->r[29].low==initial_sp-656) {
            checkMemory(rdram,0,0);return gs_interrupt.poll();
        }
        if (address == 0x10001810 && timer3_mode_supplied_ && !timer3_mode_reads) {
            checkMemory(rdram, 0, 0);
            ++timer3_mode_reads;
            return timer3_mode_;
        }
        if (address == 0x10001010 && timer2_mode_supplied_ && !timer2_mode_reads) {
            checkMemory(rdram, 0, 0);
            ++timer2_mode_reads;
            return timer2_mode_;
        }
        if (address == 0x10001010 && timer2_readback_supplied_ && timer2_mode_reads == 1 && timer2_mode_writes == 1) {
            checkMemory(rdram, 0, 0);
            if (timer2_mode_after_write != timer2_readback_expected_)
                throw std::runtime_error("Timer 2 modeled mode differs from captured readback");
            ++timer2_mode_reads;
            return timer2_mode_after_write;
        }
        return read32(rdram, address);
    }

    std::uint32_t read32(std::uint8_t* rdram, std::uint32_t address) {
        address = uncachedWordAddress(rdram, address);
        if (address == 0x1000E010 && dma_status_supplied_ && !dma_status_reads) {
            checkMemory(rdram, 0, 0);
            ++dma_status_reads;
            return dma_status_word_;
        }
        if (address == 0x1000C000 && dma5_control_supplied_ && dma_status_reads == 1 && !dma5_control_reads) {
            checkMemory(rdram, 0, 0);
            ++dma5_control_reads;
            return dma5_control_word_;
        }
        // Read-only KSEG0 view of locally supplied kernel bytes; no fabricated kernel.
        if (address >= 0x80000000 && address < 0x80000000 + kernel_snapshot_size) {
            const auto offset = address - 0x80000000;
            if (kernel_snapshot_.empty() || memory_.empty() || rdram != memory_.data()
                || offset % 4 || offset > kernel_snapshot_.size() - 4)
                throw UnsupportedGuestMemory(address, 4);
            if (const auto patched = kernel_patch_words_.find(offset); patched != kernel_patch_words_.end())
                return patched->second;
            std::uint32_t value;
            std::memcpy(&value, kernel_snapshot_.data() + offset, 4);
            return value;
        }
        checkMemory(rdram, address, 4);
        if (address % 4) throw std::runtime_error("Unsupported unaligned word read");
        std::uint32_t value;
        std::memcpy(&value, memory_.data() + address, sizeof(value));
        return value;
    }

    bool eeCheckpointDue() {
        // One observed null-bind retry executes a 2^20+1-iteration guest loop.
        if (++checkpoints_ > (post_cd.supplied ? 2000000u : 1000000u))
            throw std::runtime_error("Bootstrap execution budget exceeded; thread "+std::to_string(current_thread_id)
                +", last branch "+std::to_string(last_branch_source_)+" -> "+std::to_string(last_branch_target_));
        return false; // Run to a checked boundary, rather than pretending to schedule the EE.
    }

    std::uint32_t expected_return = 0;
    std::uint32_t syscall_resume_pc = 0;
    bool thread_initialized = false, heap_initialized = false;
    std::uint32_t heap_start = 0, heap_end = 0;
    std::uint32_t stack_base = 0, initial_sp = 0;
    std::array<std::uint32_t, 5> thread_arguments{};
    struct DormantThread {
        std::uint32_t id, entry, stack_base, stack_size, gp, initial_priority, heap_base, context_address, initial_sp;
        bool ready=false;R5900Context context{};std::uint32_t current_priority=0;bool suspended=false;
        bool waiting=false;unsigned wakeups=0; // Waiting means SleepThread; other wait kinds are unmodeled.
    };
    std::vector<DormantThread> threads;
    std::uint32_t current_thread_id = 0, main_thread_priority = 0;
    unsigned priority_changes = 0,main_thread_queries=0;
    struct StartupWorkerSpec { std::uint32_t entry,stack,size,create_return,priority_return,priority; };
    // Original independent creation/start captures establish all four identities.
    inline static constexpr std::array<StartupWorkerSpec,4> startup_worker_specs={{
        {0x2EDE58,0x34BC90,0x800,0x2EE51C,0x2EE564,8},
        {0x2EDF48,0x34D490,0x1000,0x2EE684,0x2EE6A8,16},
        {0x2EE050,0x34E490,0x1000,0x2EE70C,0x2EE730,18},
        {0x2EE140,0x34F490,0x2000,0x2EE794,0x2EE7B8,25}
    }};
    unsigned startup_worker_queries=0,startup_worker_starts=0,startup_worker_priority_changes=0;
    unsigned startup_ready_queries=0,startup_worker_suspends=0;
    unsigned disc_bind_semaphores=0,disc_rpc_binds=0,disc_rpc_calls=0;
    std::string local_disc_path;
    void receiveDiscSearch(std::uint8_t*,R5900Context*);
    unsigned cri_rpc_binds=0,cri_rpc_calls=0;
    unsigned ncmd_bind_semaphores=0,ncmd_rpc_binds=0,ncmd_ready_calls=0,ncmd_read_calls=0;
    void receiveNcmdRead(std::uint8_t*,R5900Context*);
    void receiveNcmdReady(std::uint8_t* rdram,R5900Context* context);
    bool ncmd_bind_supplied=false;
    void supplyNcmdBind(std::span<const std::uint8_t> b) {
        const std::array<unsigned,16> expected={0,0,0x80000008,0,0,0x203A97C0,0,0x369590,0x80000009,0x43418,0x43DB8,0,0,0,0,0};
        if(ncmd_bind_supplied || b.size()!=sizeof(expected) || std::memcmp(b.data(),expected.data(),sizeof(expected)))throw std::runtime_error("Invalid command bind evidence");
        ncmd_bind_supplied=true;
    }
    void receiveNcmdBind(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<unsigned>(context->r[4].low);
        if(iop_epoch!=1 || prior_iop_services_valid || context->pc!=0x32F028 || context->r[29].low!=d || d!=initial_sp-752 || context->r[5].low!=1
            || semaphores.size()!=11 || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838D8 || ncmd_bind_semaphores!=1)throw UnsupportedSyscall(0x77,context->pc);
        const std::array<unsigned,4> descriptor={0x203A97C0,0x1E640,64,0x44};
        const std::array<unsigned,9> packet={64,0,0x80000009,0,5,0x203A97C0,44,0x369590,0x80000595};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x369590)!=0x203A97C0 || read32(rdram,0x369598)!=10 || read32(rdram,0x3695A4) || read32(rdram,0x3695B4))throw UnsupportedSyscall(0x77,context->pc);
        const std::array<unsigned,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x369590,0x80000009,0x43418,0x43DB8,0,0,0,0,0};
        for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+n*4,reply[n]);
        deliverRpcEnd(rdram,context,0x369590,10,0x43418,0x43DB8);++ncmd_rpc_binds;++sif_dma_submissions;context->r[2].low=0x760001;
    }
    bool disc_bind_supplied=false;
    void supplyDiscBind(std::span<const std::uint8_t> bundle) {
        const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3A8780,0x80000009,0x433D0,0x441C8,0,0,0,0,0};
        if(disc_bind_supplied || bundle.size()!=sizeof(reply) || std::memcmp(bundle.data(),reply.data(),sizeof(reply)))
            throw std::runtime_error("Invalid disc-search bind evidence");
        disc_bind_supplied=true;
    }
    void receiveDiscBind(std::uint8_t* rdram,R5900Context* context) {
        const auto d=static_cast<std::uint32_t>(context->r[4].low);
        if(iop_epoch!=1 || !post_sif.rpc_initialized || prior_iop_services_valid || context->pc!=0x32F028
            || d!=initial_sp-736 || context->r[29].low!=d || context->r[5].low!=1 || semaphores.size()!=11
            || semaphores[10].count || semaphores[10].maximum!=1 || semaphores[10].option!=0x3838D8)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> descriptor={0x203A97C0,0x1E640,64,0x44};
        const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,42,0x3A8780,0x80000597};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+4*n)!=descriptor[n])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+4*n)!=packet[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A8780)!=0x203A97C0 || read32(rdram,0x3A8784)!=42 || read32(rdram,0x3A8788)!=10
            || read32(rdram,0x3A87A4) || read32(rdram,0x3A8794))throw UnsupportedSyscall(0x77,context->pc);
        // Validated boot-service lookup only. CDVDFSV boot entry/request is not executed here.
        const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3A8780,0x80000009,0x433D0,0x441C8,0,0,0,0,0};
        for(unsigned n=0;n<16;++n)write32(rdram,0x3A94C0+4*n,reply[n]);
        deliverRpcEnd(rdram,context,0x3A8780,10,0x433D0,0x441C8);
        ++disc_rpc_binds;++sif_dma_submissions;context->r[2].low=0x740001;
    }
    struct Semaphore { std::uint32_t count, maximum, option; };
    // Bounded bootstrap objects; no wait queues or scheduling implementation.
    std::vector<Semaphore> semaphores;
    std::uint32_t semaphore_waits = 0, semaphore_signals = 0;
    std::array<std::uint32_t, 256> syscall_handlers{};
    unsigned syscall_registrations = 0, custom_syscall_calls = 0;
    struct IntcHandler { std::uint32_t cause, address, arg, gp; };
    // Shared 128-entry pool, IDs start at one, matching pinned Play!.
    std::vector<IntcHandler> intc_handlers;
    std::vector<std::uint32_t> intc_handler_order;
    std::uint32_t timer3_interrupt_handler = 0;
    bool timer3_interrupt_enable_requested = false;
    unsigned extended_registrations = 0;
    std::uint32_t intc_mask = 0;
    unsigned intc_enable_calls = 0;
    bool startup_zero_verified = false;
    std::uint64_t startup_quadword_writes = 0, startup_byte_writes = 0;
    std::uint64_t kernel_word_writes = 0;
    unsigned timer3_mode_reads = 0, timer2_mode_reads = 0, timer2_mode_writes = 0;
    std::uint32_t timer2_mode_after_write = 0;
    unsigned cache_requests = 0, instruction_cache_invalidations = 0;
    unsigned kernel_dispatch_validations = 0;
    unsigned returns = 0, guest_calls = 0;
    // Distinct registered entry addresses; completion means a normal invocation return.
    std::set<std::uint32_t> entered_functions,completed_functions;
    std::uint64_t halfword_writes=0;
    std::uint64_t doubleword_writes = 0, word_writes = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

    // The C library per-call stdio semaphore names (options) reached by main.
    static bool stdioCallSema(std::uint32_t option) {
        return option==0x3839A0 || option==0x3839B8 || option==0x3839D0 || option==0x3839E8;
    }
    std::uint64_t overlay_dispatch_validations=0;
private:
    void receiveFirstRpcBind(std::uint8_t* rdram, R5900Context* context) {
        if (sif_rpc_reads != 1 || sif_bind_callbacks || data_cache_lines != 2
                || context->r[4].low != initial_sp - 464 || context->r[5].low != 1
                || context->r[29].low != context->r[4].low || rpc_end_callback_active_)
            throw UnsupportedSyscall(0x77, context->pc);
        const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
        const std::array<std::uint32_t, 4> expected_descriptor{0x203A97C0, 0x19600, 64, 0x44};
        const std::array<std::uint32_t, 16> request{64, 0, 0x80000009, 0, 5, 0x203A97C0, 2,
            0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0};
        for (unsigned index = 0; index < expected_descriptor.size(); ++index)
            if (read32(rdram, descriptor + index * 4) != expected_descriptor[index])
                throw UnsupportedSyscall(0x77, context->pc);
        for (unsigned index = 0; index < request.size(); ++index)
            if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
                throw UnsupportedSyscall(0x77, context->pc);
        if (sif_ee_receive_address != 0x3A94C0 || read32(rdram, 0x3A87A8) != 0x203A97C0
                || read32(rdram, 0x3A87AC) != 2 || read32(rdram, 0x3A87B0) != 3
                || semaphores.size() != 4 || semaphores[3].count != 0 || semaphores[3].maximum != 1
                || read32(rdram, 0x3A9620) != 0x3322E8 || read32(rdram, 0x3A9624) != 0x3AAFC0
                || read32(rdram, 0x3A9628) != 0x38BDF0 || !functions_.contains(0x3322E8)
                || !functions_.contains(0x332258) || !functions_.contains(0x32EC90))
            throw UnsupportedSyscall(0x77, context->pc);
        // One checked logical service lookup/response. No IOP instruction execution,
        // hardware DMA padding, interrupt entry, cycles or general RPC scheduler.
        std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
        sif_iop_[0x19600] = 0;
        std::copy(sif_bind_reply_.begin(), sif_bind_reply_.end(), memory_.begin() + 0x3A94C0);
        deliverRpcEnd(rdram, context);
        ++sif_bind_context_preservations;
        ++sif_bind_callbacks;
        ++sif_dma_submissions;
        context->r[2].low = 0x801;
    }
    // An IOP->EE SIF command handler call: packet header at the EE command buffer (0x3A94C0),
    // run on a CPU copy below the interrupted stack, as RPC_END is.
    void deliverEeCommand(std::uint8_t* rdram,R5900Context* context,std::uint32_t handler,std::uint32_t argument) {
        std::array<std::uint8_t,sizeof(R5900Context)> saved_cpu;std::memcpy(saved_cpu.data(),context,saved_cpu.size());
        auto callback=*context;callback.r[4].low=0x3A94C0;callback.r[5].low=argument;callback.r[28].low=0x38BDF0;
        constexpr std::uint32_t callback_return=0xFFFFFFF8;callback.r[31].low=callback_return;
        const auto scratch=static_cast<std::uint32_t>(callback.r[29].low)-128;checkMemory(rdram,scratch,128);
        std::array<std::uint8_t,128> saved_stack;std::copy_n(memory_.data()+scratch,128,saved_stack.data());
        rpc_end_callback_active_=true;
        try {dispatchGuestBranch(rdram,&callback,handler,context->pc,callback_return,GuestBranchKind::DirectCall,"FILEIO SIF command");}
        catch(...) {rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);throw;}
        rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);
        if(callback.r[29].low!=context->r[29].low || std::memcmp(saved_cpu.data(),context,saved_cpu.size()))
            throw std::runtime_error("FILEIO SIF command handler changed interrupted CPU state");
    }
    void deliverRpcEnd(std::uint8_t* rdram, R5900Context* context,
                       std::uint32_t client = 0x3A87A8, unsigned semaphore = 3,
                       std::uint32_t server = 0x3F570, std::uint32_t buffer = 0x3FB20) {
        std::array<std::uint8_t, sizeof(R5900Context)> saved_cpu;
        std::memcpy(saved_cpu.data(), context, saved_cpu.size());
        auto callback = *context;
        callback.r[4].low = 0x3A94C0;
        callback.r[5].low = 0x3AAFC0;
        callback.r[28].low = 0x38BDF0;
        constexpr std::uint32_t callback_return = 0xFFFFFFF8;
        callback.r[31].low = callback_return;
        // Verified handler uses a 64-byte frame below the caller SP. Preserve
        // that scratch window and execute on a CPU copy, retaining every register.
        const auto scratch = static_cast<std::uint32_t>(callback.r[29].low) - 64;
        checkMemory(rdram, scratch, 64);
        std::array<std::uint8_t, 64> saved_stack;
        std::copy_n(memory_.data() + scratch, 64, saved_stack.data());
        rpc_end_callback_active_ = true;
        try {
            dispatchGuestBranch(rdram, &callback, 0x3322E8, context->pc, callback_return,
                GuestBranchKind::DirectCall, "checked logical RPC_END");
        } catch (...) {
            rpc_end_callback_active_ = false;
            std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
            throw;
        }
        rpc_end_callback_active_ = false;
        std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
        if (callback.r[29].low != context->r[29].low || (semaphore ? semaphores[semaphore].count != 1 :
                    ((client != 0x3A6F80 && client != 0x369590 && client != 0x3A0460 && client != 0x3A0438) || read32(rdram,client+8) != UINT32_MAX))
                || read32(rdram, client) != 0 || read32(rdram, client + 36) != server
                || read32(rdram, client + 20) != buffer || read32(rdram, 0x3A97D0) != 4
                || read32(rdram, 0x3A97D8) != 0)
            throw std::runtime_error("Generated RPC_END effects differ from capture");
        if (std::memcmp(saved_cpu.data(), context, saved_cpu.size()))
            throw std::runtime_error("RPC_END changed interrupted CPU state");
    }
#ifdef BT2_NATIVE_IOP
    void receiveNativeModule(std::uint8_t* rdram,R5900Context* context);
    void receiveMcRpc(std::uint8_t*,R5900Context*);
    void receiveCriControl(std::uint8_t*,R5900Context*);
    void receiveCriStream(std::uint8_t*,R5900Context*);
    void receiveScmdCall(std::uint8_t*,R5900Context*);
    bool isCriStreamSubmission(std::uint8_t*,const R5900Context*);
    void receiveCriCreate(std::uint8_t*,R5900Context*);
    void receiveCriBind(std::uint8_t*,R5900Context*);
    void receiveDbcBind(std::uint8_t* rdram,R5900Context* context);
    void receiveDbcPort(std::uint8_t* rdram,R5900Context* context);
    void receiveDbcMain(std::uint8_t* rdram,R5900Context* context);

    void receiveSoundRpc(std::uint8_t* rdram,R5900Context* context);
    void receiveHeapFree(std::uint8_t* rdram,R5900Context* context);
#endif
    void receiveFreshLoadfile(std::uint8_t* rdram,R5900Context* context) {
        const bool bind=!post_loadfile.binds;const auto d=static_cast<std::uint32_t>(context->r[4].low);
        if(context->r[29].low!=d || d!=initial_sp-(bind?528:608) || context->r[5].low!=1
            || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
            || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,9> request={64,0,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,bind?7u:8u,0x3ACC00,bind?0x80000006u:255u};
        for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3ACC00)!=0x203A97C0 || read32(rdram,0x3ACC04)!=(bind?7u:8u) || read32(rdram,0x3ACC08)!=7)throw UnsupportedSyscall(0x77,context->pc);
        if(!bind) {
            const std::array<std::uint32_t,5> args={0,0x3ACA00,4,1,0x22568};
            for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
            std::copy_n(post_loadfile.replies.data()+128,4,memory_.data()+0x3ACA00);
        }
        std::copy_n(post_loadfile.replies.data()+(bind?0:64),64,memory_.data()+0x3A94C0);
        deliverRpcEnd(rdram,context,0x3ACC00,7,0x22568,0x225B0);
        if(bind)++post_loadfile.binds;else ++post_loadfile.versions;
        ++sif_dma_submissions;context->r[2].low=0x30000+post_loadfile.operations();
    }
    void receiveHeapBind(std::uint8_t* rdram,R5900Context* context);
    void receiveHeapAllocation(std::uint8_t* rdram,R5900Context* context);
    void receiveHeapCopy(std::uint8_t* rdram,R5900Context* context);
    void receiveIopUpload(std::uint8_t* rdram,R5900Context* context);
    void receiveFileioBind(std::uint8_t* rdram,R5900Context* context);
    void receiveFileioCall(std::uint8_t* rdram,R5900Context* context);
    void receiveFreshScmd(std::uint8_t* rdram,R5900Context* context) {
        const bool bind=!post_cd.scmd_binds;const auto d=static_cast<std::uint32_t>(context->r[4].low);
        if(context->r[29].low!=d || d!=initial_sp-(bind?432:448) || context->r[5].low!=(bind?1u:2u)
            || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
            || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || iop_epoch!=1 || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+(bind?0:16)+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,9> request={bind?64u:0x440u,bind?0u:0x439A8u,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,bind?5u:6u,0x369F40,bind?0x80000593u:34u};
        for(unsigned n=0;n<9;++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x369F40)!=0x203A97C0 || read32(rdram,0x369F44)!=(bind?5u:6u) || read32(rdram,0x369F48)!=7)
            throw UnsupportedSyscall(0x77,context->pc);
        if(!bind) {
            const std::array<std::uint32_t,4> payload={0x369A00,0x439A8,4,0};
            const std::array<std::uint32_t,5> args={4,0x3695C0,4,1,0x434F0};
            for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=payload[n])throw UnsupportedSyscall(0x77,context->pc);
            for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
            if(read32(rdram,0x369A00)!=2)throw UnsupportedSyscall(0x77,context->pc);
            write32(rdram,0x3695C0,1);
        }
        std::copy_n(post_cd.replies.data()+(bind?208:272),64,memory_.data()+0x3A94C0);
        deliverRpcEnd(rdram,context,0x369F40,7,0x434F0,0x439A8);
        if(bind)++post_cd.scmd_binds;else ++post_cd.modes;
        ++sif_dma_submissions;context->r[2].low=0x20000+post_cd.operations();
    }
    void receiveFreshCd(std::uint8_t* rdram,R5900Context* context) {
        const bool bind=post_cd.binds<2;
        auto d=static_cast<std::uint32_t>(context->r[4].low);checkMemory(rdram,d,bind?16:32);
        if(context->r[29].low!=d || d!=initial_sp-(bind?464:544) || static_cast<std::uint32_t>(context->r[5].low)!=(bind?1u:2u)
            || semaphores.size()!=8 || semaphores[7].count || semaphores[7].maximum!=1
            || semaphores[7].option!=(bind?0x3838D8u:0x3838E8u) || prior_iop_services_valid)
            throw UnsupportedSyscall(0x77,context->pc);
        const unsigned packet_offset=bind?0u:16u;
        const std::array<std::uint32_t,4> desc={0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<4;++n)if(read32(rdram,d+packet_offset+n*4)!=desc[n])throw UnsupportedSyscall(0x77,context->pc);
        const unsigned rpc_id=bind?2+post_cd.binds:4;
        const std::array<std::uint32_t,9> request={bind?64u:0x440u,bind?0u:0x441B8u,bind?0x80000009u:0x8000000Au,0,5,0x203A97C0,rpc_id,0x3A87A8,bind?0x80000592u:0u};
        for(unsigned n=0;n<request.size();++n)if(read32(rdram,0x3A97C0+n*4)!=request[n])throw UnsupportedSyscall(0x77,context->pc);
        if(read32(rdram,0x3A87A8)!=0x203A97C0 || read32(rdram,0x3A87AC)!=rpc_id || read32(rdram,0x3A87B0)!=7)
            throw UnsupportedSyscall(0x77,context->pc);
        if(!bind) {
            const std::array<std::uint32_t,4> payload={0x3A8800,0x441B8,4,0};
            const std::array<std::uint32_t,5> args={4,0x3695C0,16,1,0x43388};
            for(unsigned n=0;n<4;++n)if(read32(rdram,d+n*4)!=payload[n])throw UnsupportedSyscall(0x77,context->pc);
            for(unsigned n=0;n<5;++n)if(read32(rdram,0x3A97C0+36+n*4)!=args[n])throw UnsupportedSyscall(0x77,context->pc);
            if(read32(rdram,0x3A8800)!=0)throw UnsupportedSyscall(0x77,context->pc);
            std::copy_n(post_cd.replies.data()+192,16,memory_.data()+0x3695C0);
        }
        const unsigned reply_offset=bind?post_cd.binds*64:128;
        std::copy_n(post_cd.replies.data()+reply_offset,64,memory_.data()+0x3A94C0);
        const bool bound=!bind || post_cd.binds==1;
        deliverRpcEnd(rdram,context,0x3A87A8,7,bound?0x43388:0,bound?0x441B8:0);
        if(bind)++post_cd.binds;else ++post_cd.calls;
        ++sif_dma_submissions;
        context->r[2].low=0x20000+post_cd.binds+post_cd.calls; // authored logical token
    }
    void receiveFirstCdInit(std::uint8_t* rdram, R5900Context* context) {
        if (cd_init_calls || sif_bind_callbacks != 1 || sif_dma_submissions != 2
                || context->r[4].low != initial_sp - 544 || context->r[29].low != context->r[4].low
                || context->r[5].low != 2 || data_cache_lines != 6 || semaphores.size() != 4
                || semaphores[3].count != 0 || semaphores[3].maximum != 1
                || semaphores[3].option != 0x3838E8 || read32(rdram, 0x3A8800) != 0
                || read32(rdram, 0x3A87A8) != 0x203A97C0 || read32(rdram, 0x3A87AC) != 3
                || read32(rdram, 0x3A87B0) != 3 || read32(rdram, 0x3A87C4) != 0
                || read32(rdram, 0x3A87C8) != 0)
            throw UnsupportedSyscall(0x77, context->pc);
        const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
            0x203A97C0, 0x19600, 64, 0x44};
        const std::array<std::uint32_t, 16> request{0x440, 0x3FB20, 0x8000000A, 0,
            5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
        for (unsigned index = 0; index < descriptors.size(); ++index)
            if (read32(rdram, static_cast<std::uint32_t>(context->r[4].low) + index * 4) != descriptors[index])
                throw UnsupportedSyscall(0x77, context->pc);
        for (unsigned index = 0; index < request.size(); ++index)
            if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
                throw UnsupportedSyscall(0x77, context->pc);
        // One explicit standalone-ELF mode-0 completion checkpoint. Does not
        // execute retail IOP code, access a disc or provide general CD status.
        std::copy_n(memory_.data() + 0x3A8800, 4, sif_iop_.data() + 0x3FB20);
        std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
        sif_iop_[0x19600] = 0;
        std::copy_n(cd_init_bundle_.data() + 8, 64, memory_.data() + 0x3A94C0);
        std::copy_n(cd_init_bundle_.data() + 72, 16, memory_.data() + 0x3695C0);
        deliverRpcEnd(rdram, context);
        ++cd_init_calls;
        ++cd_init_context_preservations;
        ++sif_dma_submissions;
        context->r[2].low = 0x902;
    }
    void receiveScmdBind(std::uint8_t* rdram, R5900Context* context) {
        if (scmd_bind_calls || cd_init_calls != 1 || cd_semaphores_created != 4
                || context->r[4].low != initial_sp - 432 || context->r[29].low != context->r[4].low
                || context->r[5].low != 1 || semaphores.size() != 8 || semaphores[7].count != 0
                || semaphores[7].maximum != 1 || semaphores[7].option != 0x3838D8
                || read32(rdram,0x369F40) != 0x203A97C0 || read32(rdram,0x369F44) != 4
                || read32(rdram,0x369F48) != 7)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> descriptor{0x203A97C0,0x19600,64,0x44};
        const std::array<std::uint32_t,16> request{64,0,0x80000009,0,5,0x203A97C0,4,0x369F40,
            0x80000593,4,0x3695C0,16,1,0x3F570,0,0};
        for (unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])
            throw UnsupportedSyscall(0x77,context->pc);
        for (unsigned i=0;i<16;++i) if(read32(rdram,0x3A97C0+i*4)!=request[i])
            throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600); sif_iop_[0x19600]=0;
        std::copy(scmd_reply_.begin(),scmd_reply_.end(),memory_.begin()+0x3A94C0);
        deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
        ++scmd_bind_calls; ++sif_dma_submissions;
        // Authored positive logical submission handle; not a captured hardware DMA ID.
        // The synchronous bind consumes only the sign before waiting on its semaphore.
        context->r[2].low=1;
    }
    void receiveScmdMmode(std::uint8_t* rdram,R5900Context* context) {
        if(scmd_mmode_calls || scmd_bind_calls!=1 || semaphore_deletions!=3 || semaphores.size()!=8
            || semaphores[7].count!=0 || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8
            || context->r[4].low!=initial_sp-448 || context->r[29].low!=context->r[4].low || context->r[5].low!=2
            || read32(rdram,0x369A00)!=2 || read32(rdram,0x369F40)!=0x203A97C0
            || read32(rdram,0x369F44)!=5 || read32(rdram,0x369F48)!=7
            || read32(rdram,0x369F5C)!=0 || read32(rdram,0x369F60)!=0)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> descriptors{0x369A00,0x3F6D8,4,0,0x203A97C0,0x19600,64,0x44};
        const std::array<std::uint32_t,16> request{0x440,0x3F6D8,0x8000000A,0,5,0x203A97C0,5,0x369F40,34,4,0x3695C0,4,1,0x3F690,0,0};
        for(unsigned i=0;i<8;++i)if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptors[i])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned i=0;i<16;++i)if(read32(rdram,0x3A97C0+i*4)!=request[i])throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(memory_.data()+0x369A00,4,sif_iop_.data()+0x3F6D8);
        std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600);sif_iop_[0x19600]=0;
        std::copy(scmd_mmode_reply_.begin(),scmd_mmode_reply_.end(),memory_.begin()+0x3A94C0);
        std::fill_n(memory_.data()+0x3695C0,4,std::uint8_t{0});
        deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
        ++scmd_mmode_calls;++sif_dma_submissions;context->r[2].low=1;
    }
    bool gif_dma_supplied_=false;
    bool gs_interrupt_supplied_=false,gs_field_supplied_=false;
    bool gs_csr_supplied_=false,gs_imr_supplied_=false,gs_crt_supplied_=false;
    bool reset_submission_supplied_=false, reset_ready_supplied_=false;
    std::array<std::uint8_t,104> reset_request_{};
    bool sif_stop_supplied_ = false, sif_reset_subaddr_supplied_ = false, reset_smflag_supplied_ = false;
    bool scmd_mmode_supplied_ = false;
    std::array<std::uint8_t,64> scmd_mmode_reply_{};
    bool scmd_bind_supplied_ = false;
    std::array<std::uint8_t,64> scmd_reply_{};
    bool cd_init_supplied_ = false, cd_semaphores_supplied_ = false;
    std::array<std::uint8_t, 88> cd_init_bundle_{};
    bool sif_call_sema_supplied_ = false;
    std::vector<std::uint8_t> sif_bind_iop_;
    std::array<std::uint8_t, 64> sif_bind_reply_{};

    std::vector<std::uint8_t> sif_iop_;
    std::uint32_t sif_dma_result_ = 0;
    bool sif_rpc_supplied_ = false;
    bool rpc_end_callback_active_ = false;
    bool sif_command_supplied_ = false;
    std::uint32_t sif_command_word_ = 0;
    bool dma_status_supplied_ = false, dma5_control_supplied_ = false;
    std::uint32_t dma_status_word_ = 0, dma5_control_word_ = 0;
    bool osd_config_supplied = false, osd_readback_supplied = false;
    std::uint32_t osd_config_word = 0;
    std::uint64_t addIntcHandler(std::uint32_t cause, std::uint32_t address, std::uint32_t arg, std::uint32_t gp) {
        if (intc_handlers.size() == 128) return UINT64_MAX;
        intc_handlers.push_back({cause, address, arg, gp});
        const auto id = static_cast<std::uint32_t>(intc_handlers.size());
        intc_handler_order.insert(intc_handler_order.begin(), id);
        return id;
    }
    std::uint32_t startup_zero_start_ = 0, startup_zero_end_ = 0;
    void verifyKernelBinding(std::uint32_t address) {
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
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> kernel_code_bindings_;
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> overlay_code_bindings_;
    void checkMemory(std::uint8_t* rdram, std::uint32_t address, std::size_t count) const {
        if (memory_.empty() || rdram != memory_.data() || address > memory_.size() || count > memory_.size() - address)
            throw UnsupportedGuestMemory(address, count);
    }
    std::unordered_map<std::uint32_t, RecompiledFunction> functions_;
    std::vector<std::uint8_t> memory_, kernel_snapshot_, startup_kernel_payload_;
    std::unordered_map<std::uint32_t, std::uint32_t> kernel_patch_words_;
    std::uint32_t kernel_patch_start_ = 0, kernel_patch_end_ = 0;
    std::vector<std::uint32_t> call_returns_;
    unsigned checkpoints_ = 0, dispatch_depth_ = 0;
    std::uint32_t last_branch_source_ = 0, last_branch_target_ = 0;
    bool timer3_mode_supplied_ = false;
    std::uint32_t timer3_mode_ = 0, timer2_mode_ = 0;
    bool timer2_mode_supplied_ = false, intc_mask_supplied_ = false, timer2_readback_supplied_ = false;
    std::uint32_t timer2_readback_expected_ = 0;
    friend struct EeExecutionOwner;
    // Destroy before the invocation/memory members: cancellation needs both alive.
    std::shared_ptr<EeExecutionOwner> sound_execution_;
    std::vector<std::shared_ptr<EeExecutionOwner>> worker_executions_;

};
