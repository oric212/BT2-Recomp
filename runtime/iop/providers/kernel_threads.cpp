#include "providers.h"

// Kernel threads (thbase): thread creation, status queries and StartThread with its owned
// preemption scopes (CRI child, sound workers).

void install_iop_thread_providers(IopRuntime& rt) {
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
}
