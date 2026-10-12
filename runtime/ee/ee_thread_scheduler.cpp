#include "ee_runtime.h"
#include "ee_execution_owner.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

// EE thread scheduling: thread syscalls (status, priority, suspend/resume, sleep/wakeup), interrupt-
// context thread calls, and running ready workers on their retained stacks while main is parked.
// Main runs on the host stack; workers run as EeExecutionOwner stacks and park on SleepThread.

void EeRuntime::requireNoPreemption(std::uint32_t selector,const R5900Context* context) const {
    // Equal priorities keep FIFO order; only a strictly higher-priority ready thread preempts.
    // Main is runnable whenever it is not the caller; workers are parked or ready below it.
    const bool main=current_thread_id==1;
    const auto priority=main?main_thread_priority:threads.at(current_thread_id-2).current_priority;
    if(!main && !main_sleeping && main_thread_priority<priority)throw UnsupportedSyscall(selector,context->pc);
    for(const auto& t:threads)
        if(t.id!=current_thread_id && t.ready && !t.suspended && !t.waiting && t.current_priority<priority)
            throw UnsupportedSyscall(selector,context->pc);
}

bool EeRuntime::mainYieldsToReadyWorkers(std::uint8_t* rdram,R5900Context* context) {
    // A syscall from main that readies a worker above main's priority returns only after that worker
    // (and any other ready worker above main) has run until it parks again, as the kernel reschedules
    // on syscall return. Nested scheduling and interrupt context stay refused by the caller's check.
    if(current_thread_id!=1 || in_interrupt_ || startup_scheduler_active)return false;
    if(std::none_of(threads.begin(),threads.end(),[&](const auto& t){return t.ready && !t.suspended && !t.waiting && t.current_priority<main_thread_priority;}))
        return false;
    runReadyThreads(rdram,context);++main_yields;return true;
}

bool EeRuntime::sleepMainThread(std::uint8_t* rdram,R5900Context* context) {
    // Main sleeps until a WakeupThread/iWakeupThread names it (a banked wakeup returns at once).
    // While it sleeps, ready workers run at any priority and the idle CPU receives one logical
    // VBLANK_START per field; handlers and workers run in that order, as in deliverVblank.
    if(in_interrupt_ || startup_scheduler_active)throw UnsupportedSyscall(0x32,context->pc);
    if(main_wakeups){--main_wakeups;++main_sleeps;context->r[2].low=1;return true;}
    main_sleeping=true;
    try {
        runReadyThreads(rdram,context);
        for(unsigned fields=0;main_sleeping;++fields) {
            if(!(intc_mask&(1u<<2)) || fields>=600)throw UnsupportedSyscall(0x32,context->pc); // Nothing would wake main.
            deliverVblank(rdram,context);++main_sleep_fields;
        }
    } catch(...) {main_sleeping=false;throw;}
    ++main_sleeps;context->r[2].low=1;return true; // SleepThread returns the caller ID.
}

void EeRuntime::wakeMainThread() {
    if(main_sleeping)main_sleeping=false;else ++main_wakeups;
    ++main_wakes;
}

bool EeRuntime::handleScheduledThreadSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    const bool main=current_thread_id==1;
    if(main && selector==0x32)return sleepMainThread(rdram,context);
    if(!main && selector==0x30 && context->r[4].low==1) {
        // A worker queries main: WAIT while main sleeps, otherwise READY (main is never suspended here).
        const auto out=static_cast<std::uint32_t>(context->r[5].low);
        if(out%4)throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,out,48);
        const std::uint32_t status=main_sleeping?4u:2u;
        write32(rdram,out,status);context->r[2].low=status;++startup_worker_status_queries;return true;
    }
    if(!main && selector==0x33 && context->r[4].low==1) {
        // A worker wakes main. Main above the worker preempts it: not yet reached.
        if(main_sleeping && main_thread_priority<threads.at(current_thread_id-2).current_priority)throw UnsupportedSyscall(selector,context->pc);
        wakeMainThread();context->r[2].low=1;return true;
    }
    if(selector!=0x29 && selector!=0x30 && selector!=0x33 && selector!=0x37 && selector!=0x39 && (main || selector!=0x32))return false;
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
        if(!mainYieldsToReadyWorkers(rdram,context))
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
            t.suspended=false;t.ready=!t.waiting;++scheduled_resumes;
            if(!mainYieldsToReadyWorkers(rdram,context))
                try{requireNoPreemption(selector,context);}catch(...){t.suspended=true;t.ready=false;--scheduled_resumes;throw;}
        }
        context->r[2].low=t.id;return true;
    }
    if(selector==0x33) { // WakeupThread: a sleeping thread becomes ready (unless suspended); otherwise the wakeup is banked.
        if(self || !t.context.pc)throw UnsupportedSyscall(selector,context->pc);
        const bool was_waiting=t.waiting;
        if(was_waiting){t.waiting=false;t.ready=!t.suspended;}else ++t.wakeups;
        ++scheduled_wakes;
        if(!mainYieldsToReadyWorkers(rdram,context))
            try{requireNoPreemption(selector,context);}
            catch(...){if(was_waiting){t.waiting=true;t.ready=false;}else --t.wakeups;--scheduled_wakes;throw;}
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

bool EeRuntime::handleInterruptSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    // Interrupt context: current_thread_id names the interrupted thread; no reschedule happens here.
    const auto id=static_cast<std::uint32_t>(context->r[4].low);
    if(selector==0xFFFFFFD1u) {context->r[2].low=current_thread_id;return true;} // iGetThreadId
    if(selector==0xFFFFFFCCu && id==1) {wakeMainThread();context->r[2].low=1;return true;} // iWakeupThread(main)
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

void EeRuntime::runReadyThreads(std::uint8_t*,R5900Context* parent) {
    // Main is the parent host stack. Ready workers above main run in priority order: a new
    // owner starts a started worker; a parked owner resumes only after an owned wake.
    const auto saved=*parent;const bool nested=startup_scheduler_active;
    if(current_thread_id!=1 || in_interrupt_)throw UnsupportedSyscall(0,parent->pc);
    startup_scheduler_active=true;
    try {
        for(;;) {
            auto next=threads.end();
            for(auto t=threads.begin();t!=threads.end();++t)
                if(t->ready && !t->suspended && !t->waiting && (main_sleeping || t->current_priority<main_thread_priority)
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

void EeRuntime::dispatchReadyStartupWorkers(std::uint8_t* rdram,R5900Context* parent) {
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
