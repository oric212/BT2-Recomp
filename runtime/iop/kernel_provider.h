#pragma once
#include "native_runtime.h"

inline void install_iop_kernel_providers(IopRuntime& rt) {
    rt.providers[{"thevent",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),12);
        const IopRuntime::Event event{memory.read(p,4),memory.read(p+4,4),memory.read(p+8,4)};
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
        if(!memory.cpu_interrupts_enabled)throw std::runtime_error("Nested IOP interrupt suspend unsupported");
        if(c.get(4))memory.write(c.get(4),1,4);
        memory.cpu_interrupts_enabled=false;return 0u;
    };
    rt.providers[{"intrman",18}]=[](IopRuntime& memory,IopContext& c) {
        if(c.get(4)>1)throw std::runtime_error("Unsupported IOP interrupt state");
        memory.cpu_interrupts_enabled=c.get(4)!=0;return 0u;
    };
    rt.providers[{"intrman",4}]=[](IopRuntime& memory,IopContext& c) {
        const auto irq=c.get(4),mode=c.get(5),handler=c.get(6),arg=c.get(7);
        if(irq!=17 || mode>3 || handler%4 || memory.interrupts.contains(irq))
            throw std::runtime_error("Unsupported IOP interrupt registration");
        memory.check(IopRuntime::physical(handler),4);
        memory.entry_targets.insert(handler);memory.interrupts.emplace(irq,IopRuntime::Interrupt{mode,handler,arg,false});return 0u;
    };
    rt.providers[{"intrman",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.interrupts.find(c.get(4));
        if(found==memory.interrupts.end())throw std::runtime_error("Enable of unregistered IOP interrupt");
        found->second.enabled=true;return 0u;
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
    rt.providers[{"thevent",10}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.events.find(c.get(4));
        const auto bits=c.get(5),mode=c.get(6),result=c.get(7);
        if(found==memory.events.end() || !bits || (mode&~0x11u))throw std::runtime_error("Invalid IOP event wait");
        if(result)memory.check(IopRuntime::physical(result),4);
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
        auto& t=thread->second;t.waiting=true;t.ready=false;t.wait_event=c.get(4);t.wait_bits=bits;t.wait_mode=mode;t.wait_result=result;
        throw IopYield{};
    };
    rt.providers[{"thbase",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto found=memory.threads.find(c.get(4));
        if(found==memory.threads.end() || found->second.ready || found->second.waiting || !memory.execute || memory.current_thread)
            throw std::runtime_error("Unsupported IOP thread start");
        auto& t=found->second;t.ready=true;t.context.pc=t.entry;t.context.set(4,c.get(5));
        t.context.set(28,t.gp);t.context.set(29,t.stack+t.stacksize-0x20);t.context.set(30,t.context.get(29));t.context.set(31,IopRuntime::sentinel);
        memory.current_thread=found->first;
        try {memory.execute(memory,t.context);memory.current_thread=0;throw std::runtime_error("Unexpected IOP thread completion");}
        catch(const IopYield&) {memory.current_thread=0;return 0u;}
        catch(...) {memory.current_thread=0;throw;}
    };
    for(unsigned ordinal:{4u,5u})rt.providers[{"secrman",ordinal}]=[ordinal](IopRuntime& memory,IopContext& c) {
        const auto handler=c.get(4);
        if(handler) {if(handler%4)throw std::runtime_error("Unaligned native security callback");memory.check(IopRuntime::physical(handler),4);}
        memory.security_handlers[ordinal]=handler;if(handler)memory.entry_targets.insert(handler);
        return c.get(2); // Callback setter is void; authentication remains unsupported.
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
