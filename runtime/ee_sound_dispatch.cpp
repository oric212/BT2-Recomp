#include "ps2_runtime.h"
#include <utility>
#ifdef BT2_NATIVE_IOP
#include "module_service.h"
#endif

void PS2Runtime::probeReadySoundThread(std::uint8_t* rdram, R5900Context* parent) {
    if(ee_sound_probe_attempted || !thread_initialized || current_thread_id!=1 || main_thread_priority!=2
        || heap_calls!=6 || cri_rpc_calls!=37 || startup_worker_starts!=4 || startup_worker_priority_changes!=4
        || startup_worker_suspends!=2 || parent->pc!=0x32EAF8 || parent->r[31].low!=0x2EEA34
        || parent->r[4].low!=1 || parent->r[5].low!=24 || threads.size()!=6
        || threads[1].id!=3 || !threads[1].ready || threads[1].suspended || threads[1].entry!=0x315380
        || threads[1].current_priority!=5)
        throw UnsupportedSyscall(0x29,parent->pc);
    auto child=threads[1].context;
    if(child.cop0_status!=0x10001) throw UnsupportedSyscall(0x29,parent->pc);
    ee_sound_probe_parent=*parent;
    ee_sound_probe_attempted=true;
    auto parent_returns=std::move(call_returns_);
    const auto parent_depth=dispatch_depth_, parent_expected=expected_return;
    call_returns_.clear();dispatch_depth_=0;expected_return=0x100250;current_thread_id=3;
    auto restore=[&] {
        active_ee_stack=nullptr;current_thread_id=1;expected_return=parent_expected;
        dispatch_depth_=parent_depth;call_returns_=std::move(parent_returns);
    };
    try {
        // The retained stack is cancelled at this diagnostic boundary. No successful
        // SleepThread return, ready-state change or priority acknowledgement is supplied.
        RetainedEeStack stack([&] {
            try { dispatchGuestBranch(rdram,&child,0x315380,parent->pc,0,GuestBranchKind::TailJump,"sound child"); }
            catch (...) {ee_sound_probe_frontier=child;throw;}
            throw std::runtime_error("Unexpected sound child exit");
        });
        active_ee_stack=&stack;
        stack.resume();
        if(stack.state()!=RetainedEeStack::State::Waiting || ee_sound_probe_waits!=1)
            throw std::runtime_error("Sound child did not retain first wait");
#ifdef BT2_NATIVE_IOP
        if(native_iop) {
            try {native_iop->completeEeSoundBind(iop_epoch,memory());}
            catch(const std::exception& error) {
                native_iop_error=error.what();native_iop_pc=native_iop->runtime.executing_pc;
            }
            native_iop_pc=native_iop->runtime.executing_pc;
            native_iop_steps=native_iop->runtime.steps;
            native_iop_targets=static_cast<unsigned>(native_iop->runtime.entries.size());
        }
#endif
        // Destructor resumes only to unwind cancellation, while child return ownership
        // is still selected. Parent invocation guards remain live and unchanged.
    } catch (...) {restore();throw;}
    restore();
    throw UnsupportedSyscall(0x32,ee_sound_probe_frontier.pc);
}
