#include "providers.h"

// Kernel time (thbase): DelayThread, SetAlarm, the system clock and its conversions, all on logical
// IOP time (one field per VBlank plus owned timed waits).

void install_iop_time_providers(IopRuntime& rt) {
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
}
