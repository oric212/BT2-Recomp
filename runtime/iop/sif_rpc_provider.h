#pragma once
#include "native_runtime.h"

inline void install_iop_sif_rpc_provider(IopRuntime& rt) {
    rt.providers[{"sifman",29}]=[](IopRuntime& m,IopContext&) {return m.sif_transport_attached?1u:0u;};
    rt.providers[{"sifcmd",14}]=[](IopRuntime& m,IopContext& c) {
        if(!m.sif_transport_attached || c.get(4))throw std::runtime_error("IOP RPC initialization needs attached fresh transport and mode zero");
        m.rpc_initialized=true;return c.get(2); // Logical compatibility service; physical SIF DMA remains unsupported.
    };
    rt.providers[{"thbase",20}]=[](IopRuntime& m,IopContext&) {
        if(!m.current_thread || !m.threads.contains(m.current_thread))throw std::runtime_error("IOP thread identity outside owned thread");
        return m.current_thread;
    };
    rt.providers[{"sifcmd",19}]=[](IopRuntime& m,IopContext& c) {
        const auto p=c.get(4),thread=c.get(5);
        if(p%4 || !m.rpc_initialized || !thread || thread!=m.current_thread || !m.threads.contains(thread) || m.rpc_queues.contains(p))
            throw std::runtime_error("Invalid IOP RPC queue ownership");
        m.check(IopRuntime::physical(p),24);
        if(m.rpc_queue_tail)m.check(IopRuntime::physical(m.rpc_queue_tail+20),4);
        for(unsigned n=0;n<6;++n)m.write(p+n*4,n?0:thread,4);
        if(m.rpc_queue_tail)m.write(m.rpc_queue_tail+20,p,4);else m.rpc_queue_head=p;
        m.rpc_queue_tail=p;
        m.rpc_queues.emplace(p,IopRuntime::RpcQueue{thread,{}});return c.get(2);
    };
    rt.providers[{"sifcmd",17}]=[](IopRuntime& m,IopContext& c) {
        const auto p=c.get(4),id=c.get(5),handler=c.get(6),buffer=c.get(7),sp=c.get(29);
        m.check(IopRuntime::physical(sp+16),12);
        const auto callback=m.read(sp+16,4),callback_buffer=m.read(sp+20,4),queue=m.read(sp+24,4);
        const auto q=m.rpc_queues.find(queue);
        if(p%4 || !m.rpc_initialized || q==m.rpc_queues.end() || q->second.thread!=m.current_thread || callback || callback_buffer || m.rpc_servers.contains(id) || !handler || handler%4)
            throw std::runtime_error("Invalid IOP RPC server registration");
        m.check(IopRuntime::physical(p),68);m.check(IopRuntime::physical(handler),4);m.check(IopRuntime::physical(buffer),4);
        if(!q->second.servers.empty())throw std::runtime_error("Multiple native RPC servers per queue not yet supported");
        const std::array<std::pair<unsigned,std::uint32_t>,8> fields={{{0,id},{4,handler},{8,buffer},{16,callback},{20,callback_buffer},{56,0},{60,0},{64,queue}}};
        for(auto [offset,value]:fields)m.write(p+offset,value,4);
        m.write(queue+8,p,4);q->second.servers.push_back(p);
        m.rpc_servers.emplace(id,IopRuntime::RpcServer{p,id,handler,buffer,queue,c.get(28)});m.entry_targets.insert(handler);
        return c.get(2);
    };
    rt.providers[{"sifcmd",22}]=[](IopRuntime& m,IopContext& c)->std::uint32_t {
        const auto queue=c.get(4);const auto q=m.rpc_queues.find(queue);const auto thread=m.threads.find(m.current_thread);
        if(!m.rpc_initialized || q==m.rpc_queues.end() || q->second.thread!=m.current_thread || thread==m.threads.end() || q->second.servers.empty())
            throw std::runtime_error("Invalid native IOP RPC loop ownership");
        if(m.read(queue+12,4))throw std::runtime_error("Native IOP RPC request execution not yet supported");
        m.write(queue+4,0,4);auto& t=thread->second;t.waiting=true;t.ready=false;t.wait_rpc=queue;
        throw IopYield{}; // Actual compiled worker parks here; no request/result is fabricated.
    };
}
