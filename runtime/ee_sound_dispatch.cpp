#include "ps2_runtime.h"
#include <utility>
#include <cstdlib>
#include <algorithm>
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

void PS2Runtime::requireNoPreemption(std::uint32_t selector,const R5900Context* context) const {
    // Equal priorities keep FIFO order; only a strictly higher-priority ready thread preempts.
    // Main is runnable whenever it is not the caller; workers are parked or ready below it.
    const bool main=current_thread_id==1;
    const auto priority=main?main_thread_priority:threads.at(current_thread_id-2).current_priority;
    if(!main && main_thread_priority<priority)throw UnsupportedSyscall(selector,context->pc);
    for(const auto& t:threads)
        if(t.id!=current_thread_id && t.ready && !t.suspended && !t.waiting && t.current_priority<priority)
            throw UnsupportedSyscall(selector,context->pc);
}

bool PS2Runtime::handleScheduledThreadSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    const bool main=current_thread_id==1;
    if(selector!=0x29 && selector!=0x30 && selector!=0x37 && selector!=0x39 && (main || selector!=0x32))return false;
    const auto argument=static_cast<std::uint32_t>(context->r[4].low);
    const auto id=selector==0x32||!argument?current_thread_id:argument; // TH_SELF (0) names the caller.
    if(selector==0x29 && id==1) {
        // Main may change only its own priority here; its record is held outside threads.
        const auto priority=static_cast<std::uint32_t>(context->r[5].low);
        if(!main || !priority || priority>=128)throw UnsupportedSyscall(selector,context->pc);
        const auto previous=main_thread_priority;main_thread_priority=priority;
        try{requireNoPreemption(selector,context);}catch(...){main_thread_priority=previous;throw;}
        context->r[2].low=previous;++scheduled_priority_changes;return true;
    }
    // Main and unknown IDs are otherwise outside this bounded scope.
    if(id<2 || id-2>=threads.size() || threads[id-2].id!=id)throw UnsupportedSyscall(selector,context->pc);
    auto& t=threads[id-2];const bool self=id==current_thread_id;
    if(selector==0x29) {
        const auto priority=static_cast<std::uint32_t>(context->r[5].low);
        if(!priority || priority>=128 || !t.context.pc)throw UnsupportedSyscall(selector,context->pc);
        const auto previous=t.current_priority;t.current_priority=priority;
        try{requireNoPreemption(selector,context);}catch(...){t.current_priority=previous;throw;}
        context->r[2].low=previous;++scheduled_priority_changes;return true;
    }
    if(selector==0x30) {
        const auto out=static_cast<std::uint32_t>(context->r[5].low);
        if(out%4)throw UnsupportedSyscall(selector,context->pc); // Unstarted threads report DORMANT.
        checkMemory(rdram,out,48);
        const std::uint32_t status=self?1u:t.suspended?(t.waiting?0xCu:8u):t.waiting?4u:t.ready?2u:0x10u;
        // Reached callers consume only status; other descriptor fields remain unmodeled.
        write32(rdram,out,status);context->r[2].low=status;++startup_worker_status_queries;return true;
    }
    if(selector==0x37 || selector==0x39) {
        if(self || !t.context.pc || t.suspended!=(selector==0x39))throw UnsupportedSyscall(selector,context->pc);
        if(selector==0x37) {t.suspended=true;t.ready=false;++scheduled_suspends;}
        else {
            t.suspended=false;t.ready=!t.waiting;
            try{requireNoPreemption(selector,context);}catch(...){t.suspended=true;t.ready=false;throw;}
            ++scheduled_resumes;
        }
        context->r[2].low=t.id;return true;
    }
    if(selector==0x32) {
        if(t.wakeups)throw UnsupportedSyscall(selector,context->pc); // Banked wakeups are not yet reached.
        t.ready=false;t.waiting=true;t.context=*context;++startup_worker_waits;
        active_ee_stack->yield();
        // Only an owned wake readies a parked worker; the scheduler then resumes this stack.
        if(t.waiting || !t.ready || t.suspended || current_thread_id!=t.id)
            throw std::runtime_error("Unvalidated startup worker wake");
        context->r[2].low=t.id;++worker_sleep_returns;return true; // SleepThread returns the caller ID.
    }
    return false;
}

bool PS2Runtime::handleAdmittedIntcSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // VBLANK_START (cause 2) registration after main's admitted priority, and its replacement
    // during main's display re-initialization (GS reset removes it; the VSync setter re-adds it).
    if(selector!=0x10 && selector!=0x11 && selector!=0x14 && selector!=0x15)return false;
    constexpr std::uint32_t cause=2,bit=1u<<cause;
    if(static_cast<std::uint32_t>(context->r[4].low)!=cause || !intc_mask_supplied_)throw UnsupportedSyscall(selector,context->pc);
    if(selector==0x10) {
        const auto address=static_cast<std::uint32_t>(context->r[5].low);
        const auto next=static_cast<std::int32_t>(context->r[6].low);
        const bool replacement=display_reset_phase==12 && vblank_handler_registrations==1 && address==0x1006D8
            && context->pc==0x32E948 && context->r[31].low==0x311410 && context->r[29].low==initial_sp-96;
        if(!address || address%4 || next!=-1 || (vblank_handler_registrations && !replacement)
            || std::any_of(intc_handlers.begin(),intc_handlers.end(),[](const IntcHandler& h){return h.cause==cause;}))
            throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,address,4);
        if(intc_handlers.size()==128){context->r[2].low=UINT64_MAX;return true;}
        // The kernel records the caller GP and argument register; next=-1 appends to the chain tail.
        intc_handlers.push_back({cause,address,static_cast<std::uint32_t>(context->r[7].low),static_cast<std::uint32_t>(context->r[28].low)});
        const auto id=static_cast<std::uint32_t>(intc_handlers.size());intc_handler_order.push_back(id);
        // A removed slot is not reused: the original kernel also returned a fresh ID (4 -> 5).
        ++vblank_handler_registrations;if(replacement)display_reset_phase=13;context->r[2].low=id;return true;
    }
    if(selector==0x11) {
        const auto id=static_cast<std::uint32_t>(context->r[5].low);
        const auto order=std::find(intc_handler_order.begin(),intc_handler_order.end(),id);
        if(!displayResetStep(context,3,0x32E968,688) || (intc_mask&bit) || order==intc_handler_order.end() || intc_handlers.at(id-1).cause!=cause
            || intc_handlers.at(id-1).address!=0x1006D8 || context->r[31].low!=0x30FE90)throw UnsupportedSyscall(selector,context->pc);
        // Unlinked while cause 2 is disabled; both reached callers ignore the result.
        intc_handler_order.erase(order);intc_handlers.at(id-1).cause=~0u;display_reset_phase=4;context->r[2].low=0;return true;
    }
    if(selector==0x14) {
        const bool newly=!(intc_mask&bit);
        context->r[2].low=newly?1:0;intc_mask|=bit;++vblank_enable_calls;
        // VBLANK_START latches in INTC_STAT while masked. Original evidence places the first handler
        // entry after this enable and before main's next call, i.e. at the wrapper's EI.
        if(newly && !vblank_deliveries)deliverVblank(rdram,context);
        return true;
    }
    context->r[2].low=(intc_mask&bit)?1:0;intc_mask&=~bit;++vblank_disable_calls;return true;
}

bool PS2Runtime::handleInterruptSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // Interrupt context: current_thread_id names the interrupted thread; no reschedule happens here.
    const auto id=static_cast<std::uint32_t>(context->r[4].low);
    if(selector==0xFFFFFFD1u) {context->r[2].low=current_thread_id;return true;} // iGetThreadId
    if(id<2 || id-2>=threads.size() || threads[id-2].id!=id)return false;
    auto& t=threads[id-2];
    if(selector==0xFFFFFFCFu) { // iReferThreadStatus
        const auto out=static_cast<std::uint32_t>(context->r[5].low);
        if(out%4)return false;
        checkMemory(rdram,out,48);
        const std::uint32_t status=t.suspended?(t.waiting?0xCu:8u):t.waiting?4u:t.ready?2u:0x10u;
        write32(rdram,out,status);context->r[2].low=status;return true;
    }
    if(selector==0xFFFFFFCCu) { // iWakeupThread
        if(id==current_thread_id || !t.context.pc)return false;
        if(t.waiting) {t.waiting=false;t.ready=!t.suspended;}
        else ++t.wakeups;
        ++worker_wakes;context->r[2].low=id;return true;
    }
    return false;
}

void PS2Runtime::advanceLogicalField() {
    // Timer 2 runs from the startup mode write; its BIOS-era COUNT/COMP residue is not observable
    // (initial MODE already had CUE set), so the logical counter starts at COUNT 0, COMP 0.
    if(!timer2.started()) {
        if(!timer2_mode_writes)return; // Timer 2 never programmed: nothing counts.
        if(timer2_mode_writes!=1 || timer2_mode_after_write!=0x382 || !(intc_mask&(1u<<11)))throw std::runtime_error("Timer 2 startup state differs");
        timer2.start(timer2_mode_after_write,0,0);
    }
    timer2.advanceField();
}

void PS2Runtime::runIntcHandlers(std::uint8_t* rdram,R5900Context* interrupted,std::uint32_t cause,const char* name) {
    // Registered handlers in chain order with the kernel handler ABI (a0 cause, a1 argument,
    // registered GP) on the captured kernel interrupt stack.
    constexpr std::uint32_t stack=0x81FC0,callback_return=0xFFFFFFF8;
    checkMemory(rdram,stack-0x800,0x800);
    in_interrupt_=true;delivering_cause_=cause;
    try {
        for(const auto id:intc_handler_order) {
            const auto& h=intc_handlers.at(id-1);
            if(h.cause!=cause)continue;
            R5900Context ic{};ic.pc=h.address;ic.r[4].low=cause;ic.r[5].low=h.arg;ic.r[28].low=h.gp;
            ic.r[29].low=stack;ic.r[31].low=callback_return;ic.cop0_status=interrupted->cop0_status&~0x10001u;
            dispatchGuestBranch(rdram,&ic,h.address,interrupted->pc,callback_return,GuestBranchKind::DirectCall,name);
            if(static_cast<std::int32_t>(ic.r[2].low)<0)break; // A negative result ends the chain.
        }
    } catch(...) {in_interrupt_=false;delivering_cause_=0;throw;}
    in_interrupt_=false;delivering_cause_=0;
}

void PS2Runtime::deliverVblank(std::uint8_t* rdram,R5900Context* interrupted) {
    constexpr std::uint32_t cause=2;
    if(in_interrupt_ || current_thread_id!=1 || startup_scheduler_active || !(intc_mask&(1u<<cause)))
        throw UnsupportedSyscall(0x14,interrupted->pc);
    advanceLogicalField();
    // A Timer 2 event inside the elapsed field precedes this VBLANK_START. The library handler
    // only services its alarm list (empty here; callbacks are not modelled) and overflow count.
    if(timer2.interruptPending() && (intc_mask&(1u<<11))) {
        if(read32(rdram,0x36BAE8))throw UnsupportedSyscall(0x14,interrupted->pc);
        runIntcHandlers(rdram,interrupted,11,"Timer 2 handler");++timer2_deliveries;
        if(timer2.interruptPending())throw UnsupportedSyscall(0x14,interrupted->pc); // Handler must acknowledge.
    }
    ++vblank_deliveries;
#ifdef BT2_NATIVE_IOP
    // The IOP's own timer ticks for this field run before EE handlers observe the VBlank.
    if(native_iop) {
        try {native_iop->advanceSoundTimer(iop_epoch);}
        catch(const std::exception& e) {native_iop_error=e.what();native_iop_pc=native_iop->runtime.executing_pc;throw UnsupportedSyscall(0x14,interrupted->pc);}
    }
#endif
    runIntcHandlers(rdram,interrupted,cause,"VBLANK_START handler");
    runReadyThreads(rdram,interrupted); // Woken threads above the interrupted one run on return.
}

void PS2Runtime::runReadyThreads(std::uint8_t*,R5900Context* parent) {
    // Main is the parent host stack. Ready workers above main run in priority order: a new
    // owner starts a started worker; a parked owner resumes only after an owned wake.
    const auto saved=*parent;const bool nested=startup_scheduler_active;
    if(current_thread_id!=1 || in_interrupt_)throw UnsupportedSyscall(0,parent->pc);
    startup_scheduler_active=true;
    try {
        for(;;) {
            auto next=threads.end();
            for(auto t=threads.begin();t!=threads.end();++t)
                if(t->ready && !t->suspended && !t->waiting && t->current_priority<main_thread_priority
                    && (next==threads.end() || t->current_priority<next->current_priority))next=t;
            if(next==threads.end())break;
            const auto id=next->id;
            auto found=std::find_if(worker_executions_.begin(),worker_executions_.end(),[&](const auto& o){return o->id==id;});
            std::shared_ptr<EeExecutionOwner> owner;
            if(found!=worker_executions_.end()) {
                owner=*found;
                if(owner->stack->state()!=RetainedEeStack::State::Waiting)throw UnsupportedSyscall(0,parent->pc);
                ++worker_resumes;
            } else {
                if(id<4 || id>7)throw UnsupportedSyscall(0,parent->pc);
                owner=std::make_shared<EeExecutionOwner>(*this,id);worker_executions_.push_back(owner);
                ++startup_worker_dispatches;
            }
            try {owner->dispatch();}
            catch(...) {threads.at(id-2).context=owner->context;*parent=owner->context;throw;}
            if(owner->stack->state()!=RetainedEeStack::State::Waiting || !threads.at(id-2).waiting
                || std::memcmp(parent,&saved,sizeof(*parent)))
                throw std::runtime_error("Scheduler lost retained parent/worker ownership");
        }
    } catch(...) {startup_scheduler_active=nested;throw;}
    startup_scheduler_active=nested;
}

void PS2Runtime::dispatchReadyStartupWorkers(std::uint8_t* rdram,R5900Context* parent) {
    if(current_thread_id!=1 || main_thread_priority!=2 || parent->pc!=0x32EAF8
        || parent->r[4].low!=1 || parent->r[5].low!=24 || parent->r[31].low!=0x2EEA34
        || queued_sound_completions!=1 || !sound_thread_waiting || threads.size()!=6 || startup_scheduler_active)
        throw UnsupportedSyscall(0x29,parent->pc);
    // Admit main's lower priority first; every ready thread above it runs before main resumes.
    startup_dispatch_parent=*parent;main_priority_previous=main_thread_priority;
    main_thread_priority=24;++main_priority_admissions;
    runReadyThreads(rdram,parent);
    parent->r[2].low=main_priority_previous;++priority_changes;
}
