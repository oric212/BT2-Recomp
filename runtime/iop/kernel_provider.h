#pragma once
#include "native_runtime.h"

inline void install_iop_kernel_providers(IopRuntime& rt) {
    rt.providers[{"stdio",4}]=[](IopRuntime& memory,IopContext& c) {
        auto string=[&](std::uint32_t p) {
            std::string value;
            for(unsigned i=0;i<1024;++i) {const auto ch=memory.read(p+i,1);if(!ch)return value;value.push_back(static_cast<char>(ch));}
            throw std::runtime_error("Unterminated IOP printf string");
        };
        const auto format=string(c.get(4));std::string output;unsigned argument=5;
        for(unsigned i=0;i<format.size();++i) {
            if(format[i]!='%')output.push_back(format[i]);
            else {
                if(++i==format.size())throw std::runtime_error("Incomplete IOP printf conversion");
                if(format[i]=='%')output.push_back('%');
                else if(format[i]=='s' && argument<=7)output+=string(c.get(argument++));
                else if(format[i]=='d' && argument<=7)output+=std::to_string(iop_signed(c.get(argument++)));
                else throw std::runtime_error("Unsupported IOP printf conversion");
            }
            if(output.size()>4096)throw std::runtime_error("IOP printf output exceeds diagnostic bound");
        }
        memory.stdio_messages.push_back(output);return static_cast<std::uint32_t>(output.size());
    };

    rt.providers[{"sysmem",14}]=[](IopRuntime& memory,IopContext& c) {
        // No Kprintf callback is installed: SDK returns zero. Retain reached literal diagnostics.
        std::string message;
        for(unsigned i=0;i<1024;++i) {
            const auto ch=memory.read(c.get(4)+i,1);
            if(!ch) {memory.kernel_messages.push_back(message);return 0u;}
            if(ch=='%')throw std::runtime_error("Formatted IOP Kprintf unsupported");
            message.push_back(static_cast<char>(ch));
        }
        throw std::runtime_error("Unterminated IOP Kprintf message");
    };
    rt.providers[{"thevent",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),12);
        const IopRuntime::Event event{memory.read(p,4),memory.read(p+4,4),memory.read(p+8,4),memory.read(p+8,4)};
        if(event.attr!=0 && event.attr!=2)throw std::runtime_error("Unsupported event attributes");
        const auto id=memory.next_event++;memory.events.emplace(id,event);return id;
    };
    rt.providers[{"thbase",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),20);
        const auto attr=memory.read(p,4),option=memory.read(p+4,4),entry=memory.read(p+8,4);
        const auto size=memory.read(p+12,4),priority=memory.read(p+16,4);
        if(attr!=0x02000000 || size<0x130 || size>0x10000 || priority<1 || priority>126 || entry%4)
            throw std::runtime_error("Unsupported IOP thread parameters");
        memory.check(IopRuntime::physical(entry),4);
        const auto aligned=(size+255)&~255u,stack=memory.stack_cursor;
        if(aligned>0x1FE000-stack)throw std::runtime_error("IOP thread stack arena exhausted");
        auto contents=std::vector<std::uint8_t>(aligned);
        std::fill(contents.begin(),contents.end()-0x30,std::uint8_t{0xFF});memory.map(stack,contents);
        memory.write(p+12,aligned,4);memory.stack_cursor+=aligned;
        const auto id=memory.next_thread++;
        memory.threads.emplace(id,IopRuntime::Thread{attr,option,entry,stack,aligned,priority,c.get(28),{},false});
        memory.entry_targets.insert(entry);return id;
    };
    rt.providers[{"intrman",17}]=[](IopRuntime& memory,IopContext& c) {
        const auto previous=memory.cpu_interrupts_enabled?1u:0u;
        if(c.get(4))memory.write(c.get(4),previous,4);
        memory.cpu_interrupts_enabled=false;return previous?0u:static_cast<std::uint32_t>(-102);
    };
    rt.providers[{"intrman",23}]=[](IopRuntime& memory,IopContext&) {return memory.in_interrupt?1u:0u;}; // QueryIntrContext
    rt.providers[{"intrman",18}]=[](IopRuntime& memory,IopContext& c) {
        if(c.get(4)>1)throw std::runtime_error("Unsupported IOP interrupt state");
        memory.cpu_interrupts_enabled=c.get(4)!=0;return 0u;
    };
    rt.providers[{"intrman",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto irq=c.get(4),mode=c.get(5),handler=c.get(6),arg=c.get(7);
        if((irq!=17 && irq!=9 && irq!=36 && irq!=40) || mode>3 || handler%4 || memory.interrupts.contains(irq))
            throw std::runtime_error("Unsupported IOP interrupt registration");
        memory.check(IopRuntime::physical(handler),4);
        memory.entry_targets.insert(handler);memory.interrupts.emplace(irq,IopRuntime::Interrupt{mode,handler,arg,memory.enabled_interrupts.contains(irq),c.get(28)});return 0u;
    };
    rt.providers[{"intrman",7}]=[](IopRuntime& memory,IopContext& c) {
        const auto irq=c.get(4);
        if(irq!=9 && irq!=36 && irq!=40)throw std::runtime_error("Unsupported IOP interrupt disable");
        const auto found=memory.interrupts.find(irq);
        const bool enabled=memory.enabled_interrupts.contains(irq) || (found!=memory.interrupts.end() && found->second.enabled);
        const auto result=enabled?irq:static_cast<std::uint32_t>(-103);
        if(c.get(5))memory.write(c.get(5),result,4);
        memory.enabled_interrupts.erase(irq);if(found!=memory.interrupts.end())found->second.enabled=false;
        return enabled?0u:static_cast<std::uint32_t>(-103);
    };
    rt.providers[{"intrman",5}]=[](IopRuntime& memory,IopContext& c) {
        const auto irq=c.get(4);
        if(irq!=9 && irq!=36 && irq!=40)throw std::runtime_error("Unsupported IOP interrupt release");
        const auto found=memory.interrupts.find(irq);
        if(found==memory.interrupts.end())return static_cast<std::uint32_t>(-105);
        if(found->second.enabled)throw std::runtime_error("Release of enabled IOP interrupt unsupported");
        memory.interrupts.erase(found);return 0u;
    };
    rt.providers[{"intrman",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.interrupts.find(c.get(4));
        const auto irq=c.get(4);
        if(irq!=9 && irq!=17 && irq!=36 && irq!=40)throw std::runtime_error("Unsupported IOP interrupt enable");
        memory.enabled_interrupts.insert(irq);if(found!=memory.interrupts.end())found->second.enabled=true;return 0u;
    };
    const auto owned_card_dma=[](const IopRuntime& memory) {
        const auto server=memory.rpc_servers.find(0x80000400);
        return server!=memory.rpc_servers.end() && memory.rpc_queues.contains(server->second.queue)
            && memory.current_thread && memory.threads.contains(memory.current_thread)
            && memory.threads.at(memory.current_thread).entry==0x1039C
            && memory.threads.at(memory.current_thread).priority==16 && !memory.in_interrupt;
    };
    rt.providers[{"dmacman",28}]=[owned_card_dma](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4),source=IopRuntime::physical(c.get(5));
        memory.check(IopRuntime::physical(c.get(29))+16,4);
        const auto direction=memory.read(IopRuntime::physical(c.get(29))+16,4);
        const auto found=memory.dma_channels.find(channel);
        if(!owned_card_dma(memory) || c.pc!=0x10E28 || (channel!=11 && channel!=12) || source!=(channel==11?0x32020u:0x32650u) || c.get(6)!=0x24 || c.get(7)!=1 || direction!=(channel==11?1u:0u)
            || found==memory.dma_channels.end() || !found->second.enabled_known || !found->second.enabled
            || !found->second.priority_known || found->second.priority!=3 || (found->second.slice_known && (found->second.chcr&0x1000000)))
            throw std::runtime_error("IOP slice DMA outside first MCSERV/SIO2 profile");
        memory.check(source,0x90);
        auto& dma=found->second;dma.madr=source;dma.bcr=0x10024;dma.chcr=direction?0x201:0x40000200;dma.slice_known=true;
        return 1u; // Programmed slice only; no START, bytes, completion or IRQ supplied.
    };
    rt.providers[{"dmacman",32}]=[owned_card_dma](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);const auto found=memory.dma_channels.find(channel);
        if(!owned_card_dma(memory) || c.pc!=0x10E30 || (channel!=11 && channel!=12) || found==memory.dma_channels.end()
            || !found->second.slice_known || !found->second.enabled || found->second.bcr!=0x10024)
            throw std::runtime_error("SIO2 DMA START outside owned card slice");
        auto& dma=found->second;
        if(channel==11) {
            if(dma.madr!=0x32020 || dma.chcr!=0x201 || !memory.sio2_transmit.empty())
                throw std::runtime_error("SIO2 card send state differs");
            memory.check(dma.madr,144);
            const auto payload=std::vector<std::uint8_t>(memory.ram.begin()+dma.madr,memory.ram.begin()+dma.madr+144);
            auto expected=std::vector<std::uint8_t>(144);expected[0]=0x81;expected[1]=payload[1];
            const bool probe=payload[1]==0x11 && memory.sio2_parameters.at(0x1F808200)==0x100472;
            const bool auth=payload[1]==0xF3 && memory.sio2_parameters.at(0x1F808200)==0x140572;
            if(!memory.sio2_card_absent || payload!=expected || (!probe && !auth)
                || (probe && memory.cardSends()>=5) || (auth && memory.cardSends()!=5)
                || memory.sio2_card_sends!=memory.sio2_card_receives
                || memory.sio2_parameters.at(0x1F808204)!=0
                || memory.sio2_parameters.at(0x1F808250)!=0xFF020405 || memory.sio2_parameters.at(0x1F808254)!=0x5FFFF)
                throw std::runtime_error("SIO2 card probe packet/configuration differs");
            ++memory.sio2_card_sends;memory.sio2_transmit=payload;
            memory.sio2_receive.assign(144,0);std::fill_n(memory.sio2_receive.begin(),6,0xFF); // Explicit absent-card device profile, not a stored retail result.
            memory.sio2_receive_cursor=0;memory.sio2_command_status=0x1D100;
        } else {
            if(dma.madr!=0x32650 || dma.chcr!=0x40000200 || memory.sio2_transmit.size()!=144
                || memory.sio2_receive.size()!=144 || memory.sio2_receive_cursor
                || memory.sio2_card_sends!=memory.sio2_card_receives+1)
                throw std::runtime_error("SIO2 card receive state differs");
            memory.check(dma.madr,144);
            std::copy(memory.sio2_receive.begin(),memory.sio2_receive.end(),memory.ram.begin()+dma.madr);
            memory.sio2_receive_cursor=144;++memory.sio2_card_receives;
        }
        dma.madr+=144;dma.chcr|=0x1000000;
        return c.get(2); // START is void; completion/IRQ is a separate SIO2 control transition.
    };
    rt.providers[{"dmacman",33}]=[](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);
        if(channel!=11 && channel!=12)throw std::runtime_error("Unsupported IOP DMA channel");
        auto& dma=memory.dma_channels[channel];dma.priority=c.get(5)&7;dma.priority_known=true;
        return c.get(2); // Public API is void; no success result is supplied.
    };
    rt.providers[{"dmacman",34}]=[](IopRuntime& memory,IopContext& c) {
        const auto channel=c.get(4);
        if(channel!=11 && channel!=12)throw std::runtime_error("Unsupported IOP DMA channel");
        auto& dma=memory.dma_channels[channel];dma.enabled=true;dma.enabled_known=true;return c.get(2);
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
    rt.providers[{"thbase",35}]=[](IopRuntime& memory,IopContext& c) {
        if(c.pc!=0x31B48 || c.get(4)!=0x33F18 || c.get(5)!=0x29F84 || c.get(6) || memory.card_alarm.callback
            || memory.cardWakes()<1 || memory.cardWakes()>5 || memory.cardAlarmSets()!=memory.cardWakes()-1 || memory.read(0x33F18,4)!=3686 || memory.read(0x33F1C,4)
            || !memory.cpu_interrupts_enabled || memory.in_interrupt)
            throw std::runtime_error("IOP alarm outside first absent-card timeout");
        memory.check(0x29F84,4);memory.card_alarm={memory.logical_time_us+100,0x29F84,0,c.get(28)};
        ++memory.card_alarm_sets;return 0u;
    };
    // GetSystemTime: the 64-bit system clock (36.864 MHz cycles) from logical IOP time, which advances by
    // one field per VBlank and across owned timed waits (no sub-field instruction timing is claimed).
    rt.providers[{"thbase",34}]=[](IopRuntime& memory,IopContext& c) {
        const auto out=IopRuntime::physical(c.get(4));memory.check(out,8);
        const auto clock=memory.logical_time_us*36864/1000;
        memory.write(c.get(4),static_cast<std::uint32_t>(clock),4);memory.write(c.get(4)+4,static_cast<std::uint32_t>(clock>>32),4);
        return 0u;
    };
    // SysClock2USec(clock, sec, usec): 36.864 MHz cycles to whole seconds and the microsecond remainder.
    rt.providers[{"thbase",40}]=[](IopRuntime& memory,IopContext& c) {
        memory.check(IopRuntime::physical(c.get(4)),8);memory.check(IopRuntime::physical(c.get(5)),4);memory.check(IopRuntime::physical(c.get(6)),4);
        const auto clock=(std::uint64_t(memory.read(c.get(4)+4,4))<<32)|memory.read(c.get(4),4);
        const auto usec=clock/36864*1000+(clock%36864)*1000/36864;
        memory.write(c.get(5),static_cast<std::uint32_t>(usec/1000000),4);memory.write(c.get(6),static_cast<std::uint32_t>(usec%1000000),4);
        return c.get(2); // Public void function.
    };
    rt.providers[{"thbase",39}]=[](IopRuntime& memory,IopContext& c) {
        if(memory.spu2_voice.enabled && !memory.in_interrupt) {
            // USec2SysClock in a later sound command: 36.864 MHz IOP clock (100 us -> 3686, as captured).
            const auto usec=static_cast<std::uint64_t>(c.get(4)),clock=usec*36864/1000;memory.check(IopRuntime::physical(c.get(5)),8);
            memory.write(c.get(5),static_cast<std::uint32_t>(clock),4);memory.write(c.get(5)+4,static_cast<std::uint32_t>(clock>>32),4);return c.get(2);
        }
        if(c.pc!=0x31B50 || c.get(4)!=100 || c.get(5)!=0x33F18 || memory.cardWakes()<1 || memory.cardWakes()>5 || memory.cardFifo()!=memory.cardWakes())
            throw std::runtime_error("IOP clock conversion outside card timeout setup");
        memory.check(0x33F18,8);memory.write(0x33F18,3686,4);memory.write(0x33F1C,0,4);
        return c.get(2); // Public void conversion; 100us output independently captured.
    };
    rt.providers[{"thbase",33}]=[](IopRuntime& memory,IopContext& c) {
        const auto mc=memory.rpc_servers.find(0x80000400);
        if(c.pc!=0x31B40 || c.get(4)!=300 || c.get(31)!=0x29E58 || memory.cardWaits()>=5
            || mc==memory.rpc_servers.end() || !memory.rpc_queues.contains(mc->second.queue)
            || memory.rpc_queues.at(mc->second.queue).thread!=memory.current_thread
            || memory.cardSends()!=6 || memory.cardReceives()!=6)
            throw std::runtime_error("IOP DelayThread outside reached first card query");
        return memory.delayCurrent(c,300);
    };
    rt.providers[{"thbase",22}]=[](IopRuntime& memory,IopContext& c) {
        const auto id=c.get(4)?c.get(4):memory.current_thread;
        const auto found=memory.threads.find(id);
        if(found==memory.threads.end() || memory.in_interrupt)throw std::runtime_error("Unsupported IOP thread status query");
        const auto& t=found->second;const auto p=c.get(5);memory.check(IopRuntime::physical(p),68);
        const auto state=memory.current_thread==id?1u:t.waiting?4u:t.ready?2u:16u;
        const std::array<std::uint32_t,17> info={t.attr,t.option,state,t.entry,t.stack,t.stacksize,t.gp,t.priority,state==16?0u:t.priority,
            t.waiting?(t.wait_event?4u:t.wait_rpc?1u:0u):0u,t.waiting?t.wait_event:0u,0,0,0,0,0,0};
        for(unsigned n=0;n<info.size();++n)memory.write(p+n*4,info[n],4);
        return 0u;
    };
    rt.providers[{"thbase",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.threads.find(c.get(4));
        if(found==memory.threads.end() || found->second.ready || found->second.waiting || !memory.execute)
            throw std::runtime_error("Unsupported IOP thread start");
        const auto parent=memory.current_thread;
        const bool preempt=parent && found->second.priority<memory.threads.at(parent).priority;
        // Later sound commands (owned SPU2 voice scope) may start a higher-priority worker: it runs at
        // once until it parks on a real wait or returns, then the issuing handler continues.
        const bool sound_scope=preempt && memory.spu2_voice.enabled && memory.cpu_interrupts_enabled && !memory.in_interrupt;
        if(preempt && !sound_scope) {
            const auto server=memory.rpc_servers.find(0x90000200);
            if(server==memory.rpc_servers.end() || !memory.rpc_queues.contains(server->second.queue)
                || memory.rpc_queues.at(server->second.queue).thread!=parent || memory.threads.at(parent).priority!=48
                || c.pc!=0x1B35F0 || c.get(5) || found->second.entry!=0x1AC890
                || found->second.priority!=39 || found->second.gp!=0x1BCDF0 || found->second.stacksize!=0x800
                || !memory.cpu_interrupts_enabled || memory.in_interrupt)
                throw std::runtime_error("Higher-priority nested IOP start outside CRI scope");
        }
        auto& t=found->second;t.context={};t.ready=true;t.context.pc=t.entry;t.context.set(4,c.get(5));
        // As the IOP kernel's StartThread: the 0xB8-byte register-context area at the stack top is zeroed
        // (public kernel layout). Code that reads an uninitialized local in its first frame (SIO2D's port
        // scan slot) sees these zeros, not the 0xFF stack fill. The initial SP (top - 0x20) matches the
        // original: SOUNDS' server descriptor, built in its first frame, is at 0x...90 in captures.
        for(unsigned offset=0;offset<0xB8;offset+=4)memory.write(t.stack+t.stacksize-0xB8+offset,0,4);
        t.context.set(28,t.gp);t.context.set(29,t.stack+t.stacksize-0x20);t.context.set(30,t.context.get(29));t.context.set(31,IopRuntime::sentinel);
        if(sound_scope) {
            memory.current_thread=found->first;++memory.sound_worker_starts;
            try {memory.execute(memory,t.context);memory.current_thread=parent;t.ready=false;return 0u;} // Worker returned (exited).
            catch(const IopYield&) {
                memory.current_thread=parent;
                if(!t.waiting || t.ready)throw std::runtime_error("Sound worker yielded without an owned wait");
                return 0u;
            }
            catch(...) {memory.current_thread=parent;throw;}
        }
        if(preempt) {
            memory.current_thread=found->first;
            try {memory.execute(memory,t.context);memory.current_thread=parent;throw std::runtime_error("Unexpected CRI child completion");}
            catch(const IopYield&) {
                memory.current_thread=parent;
                if(!t.waiting || t.ready || t.wait_semaphore!=memory.read(0x1B4A84,4) || t.context.pc!=0x1B363C)
                    throw std::runtime_error("CRI child did not park at observed first semaphore");
                return 0u;
            }
            catch(...) {memory.current_thread=parent;throw;}
        }
        if(parent)return 0u; // Lower/equal priority child stays ready until the current worker blocks.
        memory.current_thread=found->first;
        try {memory.execute(memory,t.context);memory.current_thread=0;throw std::runtime_error("Unexpected IOP thread completion");}
        catch(const IopYield&) {
            memory.current_thread=0;
            for(;;) {
                auto next=memory.threads.end();
                for(auto candidate=memory.threads.begin();candidate!=memory.threads.end();++candidate)
                    if(candidate->second.ready && !candidate->second.waiting
                        && (next==memory.threads.end() || candidate->second.priority<next->second.priority))next=candidate;
                if(next==memory.threads.end())break;
                memory.current_thread=next->first;
                try {memory.execute(memory,next->second.context);memory.current_thread=0;throw std::runtime_error("Unexpected deferred IOP thread completion");}
                catch(const IopYield&) {memory.current_thread=0;}
                catch(...) {memory.current_thread=0;throw;}
            }
            return 0u;
        }
        catch(...) {memory.current_thread=0;throw;}
    };
    for(unsigned ordinal:{4u,5u})rt.providers[{"secrman",ordinal}]=[ordinal](IopRuntime& memory,IopContext& c) {
        const auto handler=c.get(4);
        if(handler) {if(handler%4)throw std::runtime_error("Unaligned native security callback");memory.check(IopRuntime::physical(handler),4);}
        memory.security_handlers[ordinal]=handler;if(handler)memory.entry_targets.insert(handler);
        return c.get(2); // Callback setter is void; authentication remains unsupported.
    };

    rt.providers[{"sysclib",11}]=[](IopRuntime& memory,IopContext& c) {
        const auto count=c.get(6),a=IopRuntime::physical(c.get(4)),b=IopRuntime::physical(c.get(5));
        if(count) {memory.check(a,count);memory.check(b,count);}
        for(std::uint32_t n=0;n<count;++n)if(memory.ram[a+n]!=memory.ram[b+n])
            return static_cast<std::uint32_t>(int(memory.ram[a+n])-int(memory.ram[b+n]));
        return 0u;
    };
    rt.providers[{"sysclib",12}]=[](IopRuntime& memory,IopContext& c) {
        const auto destination=c.get(4),source=c.get(5),count=c.get(6);
        const auto d=IopRuntime::physical(destination),s=IopRuntime::physical(source);
        if(count) {
            memory.check(d,count);memory.check(s,count);
            if(d<s+count && s<d+count)throw std::runtime_error("Overlapping IOP memcpy unsupported");
            std::memcpy(memory.ram.data()+d,memory.ram.data()+s,count);
        }
        return destination;
    };

    rt.providers[{"sysclib",17}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4),size=c.get(5);memory.check(IopRuntime::physical(p),size);
        for(std::uint32_t i=0;i<size;++i)memory.write(p+i,0,1);
        return c.get(2); // bzero is void.
    };
    rt.providers[{"sysclib",14}]=[](IopRuntime& memory,IopContext& c) {
        const auto destination=c.get(4),count=c.get(6);const auto p=IopRuntime::physical(destination);
        if(count) {memory.check(p,count);std::fill_n(memory.ram.begin()+p,count,static_cast<std::uint8_t>(c.get(5)));}
        return destination;
    };

    rt.providers[{"modload",13}]=[](IopRuntime& memory,IopContext& c) {
        const auto handler=c.get(4);
        if(handler) {if(handler%4)throw std::runtime_error("Unaligned KELF path callback");memory.check(IopRuntime::physical(handler),4);}
        memory.kelf_path_handler=handler;if(handler)memory.entry_targets.insert(handler);return c.get(2);
    };

    rt.providers[{"sysclib",29}]=[](IopRuntime& memory,IopContext& c) {
        const auto count=c.get(6);
        if(count>IopRuntime::ram_size)throw std::runtime_error("IOP string comparison budget exceeded");
        for(unsigned n=0;n<count;++n) {
            const auto a=memory.read(c.get(4)+n,1),b=memory.read(c.get(5)+n,1);
            if(a!=b)return a-b;if(!a)return 0u;
        }
        return 0u;
    };

}

// SleepThread / WakeupThread / iWakeupThread, installed only while a later sound command runs
// (other owned scopes install their own scoped sleep provider). A wake of a higher-priority
// sleeper runs it at once until it parks again.
inline void install_iop_sound_scope_sleep(IopRuntime& rt) {
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
