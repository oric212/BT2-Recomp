#include "native_runtime.h"

// IopRuntime logical time: timed waits, card and sound-timer alarms, and the per-field
// VBlank edges. Time advances one NTSC field per EE VBlank and across owned waits.

void IopRuntime::waitCardAlarm(IopContext& c,unsigned semaphore) {
    if(!card_alarm.callback || card_alarm.deadline<=logical_time_us || !cpu_interrupts_enabled || in_interrupt
        || !execute || !current_thread || !semaphores.contains(semaphore) || semaphores.at(semaphore).count)
        throw std::runtime_error("Card alarm wait outside owned pending timeout");
    for(const auto& [id,t]:threads)if(id!=current_thread && t.ready && !t.waiting)
        throw std::runtime_error("Card alarm wait has an undispatched ready worker");
    ++card_alarm_waits;logical_time_us=card_alarm.deadline;
    const auto saved=c;auto callback=interruptContext(c,card_alarm.callback,card_alarm.argument,card_alarm.gp);
    runInterrupt(callback);
    if(callback.pc!=sentinel || callback.get(2)!=0 || c!=saved
        || semaphores.at(semaphore).count!=1)throw std::runtime_error("Native card alarm callback/completion differs");
    card_alarm={};++card_alarm_fires;
}

std::uint32_t IopRuntime::delayCurrent(IopContext& c,unsigned usec) {
    const auto owner=current_thread;
    if(!owner || !threads.contains(owner) || !usec || usec>1000000 || timed_wait_owner
        || !cpu_interrupts_enabled || in_interrupt || !execute || logical_time_us>UINT64_MAX-usec)
        throw std::runtime_error("IOP timed wait outside owned enabled context");
    // Keep the live CPU and every native export/RPC frame on the host stack.
    // Ready workers have separate retained CPUs; the blocked caller cannot run.
    timed_context=c;timed_wait_owner=owner;timed_deadline_us=logical_time_us+usec;++timed_waits;
    try {
        unsigned dispatch_budget=0;
        for(;;) {
            auto next=threads.end();
            for(auto t=threads.begin();t!=threads.end();++t)
                if(t->first!=owner && t->second.ready && !t->second.waiting
                    && (next==threads.end() || t->second.priority<next->second.priority))next=t;
            if(next==threads.end())break;
            if(++dispatch_budget>32)throw std::runtime_error("IOP timed dispatch budget exceeded");
            current_thread=next->first;++timed_dispatches;
            try {execute(*this,next->second.context);throw std::runtime_error("Unexpected timed IOP worker completion");}
            catch(const IopYield&) {if(!next->second.waiting || next->second.ready)throw std::runtime_error("Timed IOP worker yielded without owned wait");}
            current_thread=owner;
        }
        // Logical idle advance only once all eligible ready workers have parked.
        logical_time_us=timed_deadline_us;
        if(c!=timed_context
            || !cpu_interrupts_enabled || in_interrupt)throw std::runtime_error("Suspended IOP timed CPU/context changed");
        timed_wait_owner=0;++timed_wakes;current_thread=owner;return 0u;
    } catch(...) {current_thread=owner;throw;} // Keep the pending snapshot/deadline for diagnosis.
}

void IopRuntime::advanceLogicalField() {
    if(in_interrupt || current_thread || timed_wait_owner || !cpu_interrupts_enabled)throw std::runtime_error("IOP field advance outside an idle point");
    logical_time_us+=16683;
    if(!card_alarm.callback || card_alarm.deadline>logical_time_us)return;
    if(!execute)throw std::runtime_error("IOP card alarm needs dispatch");
    auto handler=interruptContext({},card_alarm.callback,card_alarm.argument,card_alarm.gp);handler.set(29,0x1FFE00);
    runInterrupt(handler);
    if(handler.pc!=sentinel || handler.get(2))throw std::runtime_error("Idle card alarm callback/completion differs");
    card_alarm={};++card_alarm_fires;++card_alarm_idle_fires;
}

void IopRuntime::advanceSoundTimerField() {
    auto& t=sound_timer;
    if(!t.started)return;
    std::uint64_t elapsed=6150144;
    while(t.compare && elapsed) {
        const auto period=std::uint64_t(t.compare)*10;
        if(t.tenths>=period)throw std::runtime_error("IOP timer elapsed state invalid");
        const auto until_tick=period-t.tenths;
        if(elapsed<until_tick)break;
        if(in_interrupt || !execute)throw std::runtime_error("IOP timer tick outside an idle point");
        spu2_playback.advanceTenths(until_tick,spu2_sound_ram);
        elapsed-=until_tick;t.tenths=0;
        auto handler=interruptContext({},t.handler,t.common,t.gp);handler.set(29,0x1FFE00);
        const auto previous=current_thread;current_thread=0;
        try {runInterrupt(handler);}
        catch(...) {current_thread=previous;throw;}
        if(handler.pc!=sentinel)throw std::runtime_error("IOP timer handler did not return");
        t.compare=handler.get(2);++t.ticks;
        for(;;) {
            auto next=threads.end();
            for(auto c=threads.begin();c!=threads.end();++c)
                if(c->second.ready && !c->second.waiting && c->second.wait_satisfied && (next==threads.end() || c->second.priority<next->second.priority))next=c;
            if(next==threads.end())break;
            current_thread=next->first;
            try {execute(*this,next->second.context);next->second.ready=false;}
            catch(const IopYield&) {if(!next->second.waiting){current_thread=previous;throw std::runtime_error("IOP worker yielded without an owned wait");}}
            catch(...) {current_thread=previous;throw;}
        }
        current_thread=previous;
    }
    spu2_playback.advanceTenths(elapsed,spu2_sound_ram);
    t.tenths+=elapsed;
}

void IopRuntime::deliverVblankEdge(bool start) {
    if(!vblank_event)return;
    if(in_interrupt || current_thread || !execute)throw std::runtime_error("IOP VBlank edge outside an idle point");
    // Interrupt handler: iSetEventFlag readies the matching waiters, then iClearEventFlag runs in the
    // same handler; the readied threads run only after it returns (so a thread that loops back to
    // its wait sees the cleared bit and waits for the next edge).
    auto& event=events.at(vblank_event);
    event.bits|=start?3u:12u;
    std::vector<unsigned> wake;
    for(auto& [id,t]:threads) {
        if(!t.waiting || t.wait_event!=vblank_event)continue;
        const auto shared=event.bits&t.wait_bits;
        if(!((t.wait_mode&1)?shared!=0:shared==t.wait_bits))continue;
        if(t.wait_result)write(t.wait_result,event.bits,4);
        if(t.wait_mode&0x10)event.bits=0;
        t.waiting=false;t.ready=true;t.wait_satisfied=true;wake.push_back(id);
    }
    event.bits&=start?~9u:~6u;
    std::stable_sort(wake.begin(),wake.end(),[&](unsigned a,unsigned b){return threads.at(a).priority<threads.at(b).priority;});
    for(auto id:wake) {
        current_thread=id;
        try {execute(*this,threads.at(id).context);current_thread=0;throw std::runtime_error("Unexpected IOP worker completion");}
        catch(const IopYield&) {current_thread=0;}
        catch(...) {current_thread=0;throw;}
    }
    ++vblank_edges;
}
