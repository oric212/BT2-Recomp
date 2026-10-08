#include "providers.h"

// SleepThread / WakeupThread / iWakeupThread, installed only while a later sound command runs
// (other owned scopes install their own scoped sleep provider). A wake of a higher-priority
// sleeper runs it at once until it parks again.
void install_iop_sound_scope_sleep(IopRuntime& rt) {
    rt.providers[{"thbase",24}]=[](IopRuntime& memory,IopContext&) {
        const auto owner=memory.threads.find(memory.current_thread);
        if(owner==memory.threads.end() || memory.in_interrupt)throw std::runtime_error("IOP sleep outside an owned thread");
        auto& t=owner->second;
        if(t.wait_satisfied && t.wait_sleep) {t.wait_satisfied=false;t.wait_sleep=false;return 0u;}
        if(t.wakeups) {--t.wakeups;return 0u;}
        t.waiting=true;t.ready=false;t.wait_sleep=true;t.wait_order=++memory.next_wait_order;
        throw IopYield{};
    };
    auto wake=[](IopRuntime& memory,IopContext& c,bool interrupt) {
        const auto found=memory.threads.find(c.get(4));
        if(found==memory.threads.end() || c.get(4)==memory.current_thread || interrupt!=memory.in_interrupt)
            throw std::runtime_error("Unsupported IOP thread wakeup");
        auto& t=found->second;
        if(!(t.waiting && t.wait_sleep)) {++t.wakeups;return 0u;}
        t.waiting=false;t.ready=true;t.wait_satisfied=true;
        const auto parent=memory.current_thread;
        if(interrupt || !parent || t.priority>=memory.threads.at(parent).priority)return 0u; // Runs when the caller blocks.
        if(!memory.spu2_voice.enabled || !memory.cpu_interrupts_enabled || !memory.execute)
            throw std::runtime_error("Higher-priority IOP wakeup outside sound scope");
        memory.current_thread=found->first;
        try {memory.execute(memory,t.context);memory.current_thread=parent;t.ready=false;return 0u;}
        catch(const IopYield&) {
            memory.current_thread=parent;
            if(!t.waiting || t.ready)throw std::runtime_error("Woken IOP worker yielded without an owned wait");
            return 0u;
        }
        catch(...) {memory.current_thread=parent;throw;}
    };
    rt.providers[{"thbase",25}]=[wake](IopRuntime& memory,IopContext& c) {return wake(memory,c,false);};
    // TIMRMAN for the SOUNDS worker's periodic tick: AllocHardTimer(SYSCLK, 32-bit, prescale 1),
    // SetTimerHandler, SetupHardTimer, StartHardTimer. Captured placement: counter 5, id 0x5BF80149.
    rt.providers[{"timrman",4}]=[](IopRuntime& memory,IopContext& c) {
        if(memory.sound_timer.allocated || c.get(4)!=1 || c.get(5)!=32 || c.get(6)!=1 || memory.in_interrupt)
            throw std::runtime_error("Unsupported IOP hardware timer allocation");
        memory.sound_timer={};memory.sound_timer.allocated=true;memory.sound_timer.id=0x5BF80149;return memory.sound_timer.id;
    };
    rt.providers[{"timrman",20}]=[](IopRuntime& memory,IopContext& c) {
        auto& t=memory.sound_timer;
        if(!t.allocated || t.started || c.get(4)!=t.id || !c.get(5) || c.get(6)%4)throw std::runtime_error("Unsupported IOP timer handler");
        memory.check(IopRuntime::physical(c.get(6)),4);t.compare=c.get(5);t.handler=c.get(6);t.common=c.get(7);t.gp=c.get(28);return 0u;
    };
    rt.providers[{"timrman",22}]=[](IopRuntime& memory,IopContext& c) {
        const auto& t=memory.sound_timer;
        if(!t.allocated || t.started || c.get(4)!=t.id || c.get(5)!=1 || c.get(6)!=0 || c.get(7)!=1)throw std::runtime_error("Unsupported IOP timer setup");
        return 0u;
    };
    rt.providers[{"timrman",23}]=[](IopRuntime& memory,IopContext& c) {
        auto& t=memory.sound_timer;
        if(!t.allocated || t.started || c.get(4)!=t.id || !t.handler)throw std::runtime_error("Unsupported IOP timer start");
        t.started=true;t.tenths=0;return 0u;
    };
    rt.providers[{"thbase",26}]=[wake](IopRuntime& memory,IopContext& c) {return wake(memory,c,true);};
}
