#include "ps2_runtime.h"
#include <utility>
#include <cstdlib>
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

// A persistent guest invocation owns its CPU, host frames and guest return stack.
// Main remains live on the parent host stack; each selection swaps only ownership.
struct EeExecutionOwner {
    PS2Runtime& rt;
    R5900Context context;
    std::vector<unsigned> returns;
    unsigned depth=0,expected=0x100250,saved_id=0,id=3,entry=0x315380;
    RetainedEeStack* saved_active=nullptr;
    std::unique_ptr<RetainedEeStack> stack;
    explicit EeExecutionOwner(PS2Runtime& runtime,unsigned thread_id=3):rt(runtime),context(runtime.threads.at(thread_id-2).context),id(thread_id),entry(runtime.threads.at(thread_id-2).entry) {
        stack=std::make_unique<RetainedEeStack>([this] {
            rt.dispatchGuestBranch(rt.memory().data(),&context,entry,id==3?rt.sound_dispatch_wait_ra:rt.startup_dispatch_parent.pc,0,PS2Runtime::GuestBranchKind::TailJump,"owned EE sound worker");
            throw std::runtime_error("Unexpected persistent sound worker exit");
        });
    }
    void select() {
        saved_id=rt.current_thread_id;saved_active=rt.active_ee_stack;
        std::swap(returns,rt.call_returns_);std::swap(depth,rt.dispatch_depth_);std::swap(expected,rt.expected_return);
        rt.current_thread_id=id;rt.active_ee_stack=stack.get();
    }
    void restore() {
        rt.current_thread_id=saved_id;rt.active_ee_stack=saved_active;
        std::swap(returns,rt.call_returns_);std::swap(depth,rt.dispatch_depth_);std::swap(expected,rt.expected_return);
    }
    void dispatch() {select();try{stack->resume();}catch(...){restore();throw;}restore();}
    ~EeExecutionOwner() {
        if(stack && stack->state()==RetainedEeStack::State::Waiting) {
            select();stack.reset();restore();
        }
    }
};

void PS2Runtime::queueSoundsBind(R5900Context* context,const std::array<unsigned,16>& reply) {
    if(!ee_queued_sound_rpc || queued_sounds_bind.pending || queued_sound_submissions || sounds_rpc_binds
        || current_thread_id!=1 || main_thread_priority!=2 || iop_epoch!=1 || reply[7]!=0x6351C0 || reply[8]!=0x80000009)
        throw UnsupportedSyscall(0x77,context->pc);
    queued_sounds_bind={true,iop_epoch,reply};++queued_sound_submissions;
    ++sif_dma_submissions;context->r[2].low=0x60003; // Accepted transport submission only.
}
void PS2Runtime::finishQueuedSoundsBind(std::uint8_t* rdram,R5900Context* context) {
    if(!ee_queued_sound_rpc || !queued_sounds_bind.pending || queued_sound_completions || queued_sounds_bind.epoch!=iop_epoch
        || current_thread_id!=1 || main_thread_priority!=2 || context->pc!=0x32ECA8 || context->r[4].low!=7
        || context->r[31].low!=0x332874 || threads.size()!=2 || !threads[1].ready || threads[1].entry!=0x315380
        || threads[1].context.cop0_status!=0x10001 || semaphores.size()!=8 || semaphores[7].count
        || semaphores[7].option!=0x3838D8 || read32(rdram,0x3A97D8)!=29 || read32(rdram,0x3A97DC)!=0x6351C0
        || read32(rdram,0x3A97E0)!=0x02000004 || sound_execution_)
        throw UnsupportedSyscall(0x44,context->pc);
    const auto saved=*context;
    sound_dispatch_bind_id=29;sound_dispatch_wait_ra=static_cast<unsigned>(context->r[31].low);
    sound_dispatch_wait_sp=static_cast<unsigned>(context->r[29].low);
    sound_execution_=std::make_shared<EeExecutionOwner>(*this);
    sound_execution_->dispatch();
    if(!sound_thread_waiting || sound_execution_->stack->state()!=RetainedEeStack::State::Waiting
        || std::memcmp(&saved,context,sizeof(saved)) || current_thread_id!=1)
        throw std::runtime_error("Queued bind lost retained main/child ownership");
#ifdef BT2_NATIVE_IOP
    if(!native_iop) throw UnsupportedSyscall(0x44,context->pc);
    native_iop->ownEeSoundServer(iop_epoch,memory());
#endif
    const auto reply=queued_sounds_bind.reply;
    for(unsigned n=0;n<reply.size();++n)write32(rdram,0x3A94C0+n*4,reply[n]);
    deliverRpcEnd(rdram,context,0x6351C0,7,reply[9],reply[10]);
    queued_sounds_bind.pending=false;++queued_sound_completions;++sounds_rpc_binds;
    if(std::memcmp(&saved,context,sizeof(saved))) throw std::runtime_error("Queued completion changed waiting main CPU");
}

void PS2Runtime::dispatchReadyStartupWorkers(std::uint8_t*,R5900Context* parent) {
    // Investigation scope only: final priority admission/critical preemption remains pending.
    if(!std::getenv("BT2_EE_STARTUP_WORKER_PROBE"))throw UnsupportedSyscall(0x29,parent->pc);
    if(current_thread_id!=1 || main_thread_priority!=2 || parent->pc!=0x32EAF8
        || parent->r[4].low!=1 || parent->r[5].low!=24 || parent->r[31].low!=0x2EEA34
        || queued_sound_completions!=1 || !sound_thread_waiting || threads.size()!=6)
        throw UnsupportedSyscall(0x29,parent->pc);
    startup_dispatch_parent=*parent;
    for(;;) {
        auto next=threads.end();
        for(auto t=threads.begin();t!=threads.end();++t)
            if(t->ready && !t->suspended && t->current_priority<24
                && (next==threads.end() || t->current_priority<next->current_priority))next=t;
        if(next==threads.end())break;
        if(next->id!=5 && next->id!=6)throw UnsupportedSyscall(0x29,parent->pc);
        const auto id=next->id;
        auto owner=std::make_shared<EeExecutionOwner>(*this,id);worker_executions_.push_back(owner);
        ++startup_worker_dispatches;
        try {owner->dispatch();}
        catch(...) {threads.at(id-2).context=owner->context;*parent=owner->context;throw;}
        if(owner->stack->state()!=RetainedEeStack::State::Waiting || threads.at(id-2).ready
            || std::memcmp(parent,&startup_dispatch_parent,sizeof(*parent)))
            throw std::runtime_error("Startup dispatch lost retained parent/worker ownership");
    }
    if(startup_worker_dispatches!=2 || startup_worker_waits!=2)throw UnsupportedSyscall(0x29,parent->pc);
    parent->r[2].low=main_thread_priority;main_thread_priority=24;++priority_changes;
}
