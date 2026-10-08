#include "providers.h"

// Kernel synchronization: thsemap semaphores and thevent event flags, including the owned
// waits (worker parking, card alarm wait) and event-flag wakeups.

void install_iop_sync_providers(IopRuntime& rt) {
    rt.providers[{"thevent",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),12);
        const IopRuntime::Event event{memory.read(p,4),memory.read(p+4,4),memory.read(p+8,4),memory.read(p+8,4)};
        if(event.attr!=0 && event.attr!=2)throw std::runtime_error("Unsupported event attributes");
        const auto id=memory.next_event++;memory.events.emplace(id,event);return id;
    };
    rt.providers[{"thsemap",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),16);
        const IopRuntime::Semaphore sem{memory.read(p,4),memory.read(p+4,4),memory.read(p+8,4),memory.read(p+12,4)};
        if((sem.attr!=0 && sem.attr!=1) || sem.maximum==0 || sem.maximum>INT32_MAX || sem.count>sem.maximum)
            throw std::runtime_error("Unsupported IOP semaphore parameters");
        const auto id=memory.next_semaphore++;memory.semaphores.emplace(id,sem);return id;
    };
    rt.providers[{"thsemap",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.semaphores.find(c.get(4));
        if(found==memory.semaphores.end() || found->second.count>=found->second.maximum)throw std::runtime_error("Invalid IOP semaphore signal");
        ++found->second.count;return 0u; // Zero-count suspension/wait queues remain unsupported.
    };
    rt.providers[{"thsemap",7}]=[](IopRuntime& memory,IopContext& c) {
        // iSignalSema from interrupt context with no thread waiting on the semaphore: the count rises
        // (bounded by its maximum). Waking a waiting thread from an interrupt is not modelled yet.
        const auto found=memory.semaphores.find(c.get(4));
        if(!memory.in_interrupt || memory.cpu_interrupts_enabled || found==memory.semaphores.end()
            || found->second.count>=found->second.maximum)
            throw std::runtime_error("IOP interrupt semaphore signal outside an owned interrupt");
        for(const auto& [id,t]:memory.threads)
            if(t.waiting && t.wait_semaphore==c.get(4))throw std::runtime_error("IOP interrupt semaphore wake of a waiting thread unsupported");
        ++found->second.count;return 0u;
    };
    rt.providers[{"thsemap",8}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.semaphores.find(c.get(4));
        if(found==memory.semaphores.end())throw std::runtime_error("Unknown IOP semaphore wait");
        if(!memory.cpu_interrupts_enabled)throw std::runtime_error("IOP semaphore wait with interrupts disabled");
        if(!found->second.count) {
            const auto owner=memory.threads.find(memory.current_thread);
            if(owner!=memory.threads.end() && owner->second.entry==0x1AC890 && owner->second.priority==39
                && owner->second.gp==0x1BCDF0 && c.pc==0x1B363C && c.get(31)==0x1AC8D0
                && c.get(4)==memory.read(0x1B4A84,4) && !memory.in_interrupt) {
                auto& t=owner->second;t.waiting=true;t.ready=false;t.wait_semaphore=c.get(4);
                throw IopYield{}; // Preserve the original import continuation for a future owned wake.
            }
            if(c.pc!=0x31B94 || c.get(4)!=memory.read(0x33F10,4) || memory.card_alarm.callback!=0x29F84
                || memory.cardAlarmFires()>=5 || memory.cardAlarmWaits()>=5
                || memory.cardWakes()!=memory.cardAlarmWaits()+1)
                throw std::runtime_error("IOP semaphore suspension requires retained native call frames: pc="+std::to_string(c.pc)+" sema="+std::to_string(c.get(4))
                    +" ra="+std::to_string(c.get(31))+" thread="+std::to_string(memory.current_thread)+" alarm="+std::to_string(memory.card_alarm.callback)
                    +" fires="+std::to_string(memory.cardAlarmFires())+" waits="+std::to_string(memory.cardAlarmWaits())+" wakes="+std::to_string(memory.cardWakes()));
            memory.waitCardAlarm(c,c.get(4));
        }
        --found->second.count;return 0u;
    };
    rt.providers[{"thevent",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.events.find(c.get(4));
        if(found==memory.events.end())throw std::runtime_error("Unknown IOP event set");
        if(!c.get(5))return 0u;
        auto bits=found->second.bits|c.get(5);
        std::vector<unsigned> ordered;
        for(const auto& [id,t]:memory.threads)if(t.waiting && t.wait_event==c.get(4))ordered.push_back(id);
        std::stable_sort(ordered.begin(),ordered.end(),[&](unsigned a,unsigned b){return memory.threads.at(a).wait_order<memory.threads.at(b).wait_order;});
        std::vector<unsigned> wake;
        for(auto id:ordered) {
            auto& t=memory.threads.at(id);const auto shared=bits&t.wait_bits;
            if(!((t.wait_mode&1)?shared!=0:shared==t.wait_bits))continue;
            if(!memory.execute)throw std::runtime_error("IOP event wake needs dispatch");
            if(memory.current_thread) {
                // A running thread starting an SIO2MAN transfer wakes SIO2MAN's worker (priority 16), which
                // preempts the caller at once. Callers so far: MCSERV (104) and the pad threads (DS2U_D/SIO2D).
                const bool sio2_preemption=memory.threads.at(memory.current_thread).priority>t.priority && t.priority==16
                    && t.entry==0x1039C && t.wait_event==0x10001 && id!=memory.current_thread
                    && c.pc==0x10EA8 && memory.cpu_interrupts_enabled && !memory.in_interrupt;
                if(!sio2_preemption)throw std::runtime_error("Nested IOP event wake outside the SIO2MAN worker scope");
            }
            if(t.wait_result)memory.check(IopRuntime::physical(t.wait_result),4);
            wake.push_back(id);if(t.wait_mode&0x10)bits=0;
        }
        found->second.bits|=c.get(5);
        for(auto id:wake) {
            auto& t=memory.threads.at(id);
            if(t.wait_result)memory.write(t.wait_result,found->second.bits,4);
            if(t.wait_mode&0x10)found->second.bits=0;
            t.waiting=false;t.ready=true;t.wait_satisfied=true;
        }
        std::stable_sort(wake.begin(),wake.end(),[&](unsigned a,unsigned b){return memory.threads.at(a).priority<memory.threads.at(b).priority;});
        const auto caller_thread=memory.current_thread;
        for(auto id:wake) {
            memory.current_thread=id;
            try {memory.execute(memory,memory.threads.at(id).context);memory.current_thread=caller_thread;throw std::runtime_error("Unexpected IOP worker completion");}
            catch(const IopYield&) {memory.current_thread=caller_thread;}
            catch(...) {memory.current_thread=caller_thread;throw;}
        }
        return 0u;
    };
    rt.providers[{"thevent",7}]=[](IopRuntime& memory,IopContext& c) {
        if(!memory.in_interrupt)throw std::runtime_error("IOP interrupt event set outside interrupt");
        return memory.providers.at({"thevent",6})(memory,c);
    };
    rt.providers[{"thevent",8}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.events.find(c.get(4));
        if(found==memory.events.end())throw std::runtime_error("Unknown IOP event clear");
        found->second.bits&=c.get(5);return 0u;
    };
    // ReferEventFlagStatus (13, thread context) / iReferEventFlagStatus (14, interrupt context):
    // attr, option, initial bits, current bits, waiter count (public iop_event_info_t layout).
    for(unsigned ordinal:{13u,14u})rt.providers[{"thevent",ordinal}]=[ordinal](IopRuntime& memory,IopContext& c) {
        if((ordinal==14)!=memory.in_interrupt)return static_cast<std::uint32_t>(-100); // KE_ILLEGAL_CONTEXT.
        const auto found=memory.events.find(c.get(4));
        if(found==memory.events.end())return static_cast<std::uint32_t>(-409); // KE_UNKNOWN_EVFID.
        const auto info=c.get(5);memory.check(IopRuntime::physical(info),28);
        unsigned waiters=0;for(const auto& [id,t]:memory.threads)if(t.waiting && t.wait_event==c.get(4))++waiters;
        const auto& e=found->second;
        for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,5>{{{0,e.attr},{4,e.option},{8,e.init_bits},{12,e.bits},{16,waiters}}})
            memory.write(info+offset,value,4);
        return 0u;
    };
    rt.providers[{"thevent",10}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.events.find(c.get(4));
        const auto bits=c.get(5),mode=c.get(6),result=c.get(7);
        if(found==memory.events.end() || !bits || (mode&~0x11u))throw std::runtime_error("Invalid IOP event wait");
        if(result)memory.check(IopRuntime::physical(result),4);
        const auto owner=memory.threads.find(memory.current_thread);
        if(owner!=memory.threads.end() && owner->second.wait_satisfied) {
            auto& t=owner->second;
            if(t.wait_event!=c.get(4) || t.wait_bits!=bits || t.wait_mode!=mode || t.wait_result!=result)
                throw std::runtime_error("IOP satisfied wait context differs");
            t.wait_satisfied=false;t.wait_event=0;return 0u;
        }
        auto& event=found->second;const auto shared=event.bits&bits;
        if((mode&1)?shared!=0:shared==bits) {
            if(result)memory.write(result,event.bits,4);
            if(mode&0x10)event.bits=0;
            const auto owner=memory.threads.find(memory.current_thread);
            if(owner!=memory.threads.end()) {owner->second.waiting=false;owner->second.ready=true;owner->second.wait_event=0;}
            return 0u;
        }
        const auto thread=memory.threads.find(memory.current_thread);
        if(thread==memory.threads.end())throw std::runtime_error("IOP event wait outside owned thread");
        for(const auto& [id,t]:memory.threads)if(t.waiting && t.wait_event==c.get(4) && id!=memory.current_thread && event.attr==0)
            throw std::runtime_error("Multiple waiters on single IOP event");
        auto& t=thread->second;t.waiting=true;t.ready=false;t.wait_event=c.get(4);t.wait_bits=bits;t.wait_mode=mode;t.wait_result=result;t.wait_order=++memory.next_wait_order;
        throw IopYield{};
    };
}
