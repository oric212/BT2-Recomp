#include "providers.h"

// Kernel interrupts (intrman): CPU interrupt state, handler registration, enable/disable/release.

void install_iop_interrupt_providers(IopRuntime& rt) {
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
}
