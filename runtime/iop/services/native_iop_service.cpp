#include "module_service.h"
#include <cstdlib>

// NativeIopService core: construction, module loading, registered RPC execution on the borrowed
// server worker, and the per-field IOP advance (VBlank edges, logical time and pending alarms).

NativeIopService::NativeIopService(unsigned epoch,std::vector<NativeIopModule> definitions):epoch_(epoch),definitions_(std::move(definitions)) {
    if(!epoch_)throw std::runtime_error("Native IOP needs a fresh epoch");
    runtime.spu2_playback.enabled=true;
    if(const char* watch=std::getenv("BT2_IOP_WATCH"))runtime.watch_address=static_cast<std::uint32_t>(std::stoul(watch,nullptr,16));
    if(const char* script=std::getenv("BT2_PAD_SCRIPT"))runtime.pad.script=PadScript::parse(script); // Repeatable input (pad_script.h).
    runtime.map(0x1FF000,std::vector<std::uint8_t>(0x1000));
    runtime.map(0x1FE000,std::vector<std::uint8_t>(0x1000));
    install_iop_library_provider(runtime);install_iop_kernel_providers(runtime);install_iop_ioman_provider(runtime);
    install_iop_sif_rpc_provider(runtime);install_iop_vblank_provider(runtime);install_iop_cdvd_provider(runtime);
    runtime.execute=[this](IopRuntime& rt,IopContext& c){
        while(c.pc!=IopRuntime::sentinel) {
            rt.check(IopRuntime::physical(c.pc),4);
            const auto before=c.pc;bool dispatched=false;
            for(const auto& module:definitions_)if(IopRuntime::physical(c.pc)>=module.base && IopRuntime::physical(c.pc)<module.base+module.image.size()) {
                module.execute(rt,c);dispatched=true;break;
            }
            if(!dispatched || c.pc==before)throw std::runtime_error("Unmapped or stalled native IOP module dispatch");
        }
    };
}

NativeIopService::Resident NativeIopService::load(unsigned epoch,std::string_view path,std::span<const std::uint8_t> arguments) {
    if(epoch!=epoch_ || failed_ || !arguments.empty() || residents.contains(std::string(path)))
        throw std::runtime_error("Invalid native IOP epoch, arguments, state or duplicate load");
    const NativeIopModule* found=nullptr;
    for(const auto& module:definitions_)if(module.path==path){found=&module;break;}
    if(!found)throw std::runtime_error("No compiled native IOP module for requested path: "+std::string(path));
    if(path.size()>255 || path.find('\0')!=std::string_view::npos)throw std::runtime_error("Invalid native IOP module path");
    try {
        runtime.map(found->base,found->image);found->exports(runtime);
        for(unsigned n=0;n<path.size();++n)runtime.write(0x1FE100+n,static_cast<unsigned char>(path[n]),1);
        runtime.write(0x1FE100+static_cast<std::uint32_t>(path.size()),0,1);
        runtime.write(0x1FE000,0x1FE100,4);runtime.write(0x1FE004,0,4);
        entry={};entry.pc=found->entry;entry.set(28,found->gp);entry.set(29,0x1FFF00);
        entry.set(31,IopRuntime::sentinel);entry.set(4,1);entry.set(5,0x1FE000);
        runtime.execute(runtime,entry);
        if(entry.pc!=IopRuntime::sentinel || (entry.get(2)!=0 && entry.get(2)!=2))throw std::runtime_error("IOP module did not return resident completion: "+std::to_string(entry.get(2)));
        found->validate(runtime);
        const Resident resident{next_id_++,entry.get(2)};residents.emplace(std::string(path),resident);return resident;
    } catch(...) {failed_=true;throw;}
}

std::vector<std::uint8_t> NativeIopService::executeRegisteredRpc(std::uint32_t service,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t client,std::uint32_t result) {
    const auto found=runtime.rpc_servers.find(service);
    if(found==runtime.rpc_servers.end())throw std::runtime_error("No owned registered RPC server");
    const auto server=found->second;const auto queue=runtime.rpc_queues.find(server.queue);
    if(queue==runtime.rpc_queues.end() || runtime.current_thread || !runtime.cpu_interrupts_enabled)
        throw std::runtime_error("registered RPC request outside idle owned context");
    auto& worker=runtime.threads.at(queue->second.thread);
    if(!worker.waiting || worker.ready || worker.wait_rpc!=server.queue || worker.gp!=server.gp)
        throw std::runtime_error("registered RPC worker is not parked at its queue");
    runtime.check(server.buffer,static_cast<unsigned>(input.size()));
    runtime.check(worker.context.get(29)-64,64);
    runtime.check(server.descriptor,68);runtime.check(server.queue,24);
    if(runtime.read(server.queue+12,4) || runtime.read(server.queue+4,4))throw std::runtime_error("registered RPC queue already has a request");
    try {
        std::copy(input.begin(),input.end(),runtime.ram.begin()+server.buffer);
        // Reached SDK request fields; link/end retain ownership after dequeue.
        for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,8>{{{12,static_cast<unsigned>(input.size())},{28,client},{32,0x203A97C0},{36,command},{40,result},{44,result_size},{48,1},{52,5}}})
            runtime.write(server.descriptor+offset,value,4);
        runtime.write(server.queue+16,server.descriptor,4);runtime.write(server.queue+12,0,4);runtime.write(server.queue+4,1,4);
        // The public RPC loop calls the registered handler on its worker stack.
        // Retain the parked loop CPU; this bounded synchronous request borrows it.
        IopContext c=worker.context;c.pc=server.handler;c.branch_pending=false;c.delay_instruction=false;c.load_register=0;
        c.set(4,command);c.set(5,server.buffer);c.set(6,static_cast<unsigned>(input.size()));c.set(28,server.gp);c.set(31,IopRuntime::sentinel);
        // Waits the handler satisfies at once (e.g. SIO2MAN completion on WaitEventFlag) update the borrowed
        // thread's wait fields; once the handler returns, the worker is back at its queue receive.
        const auto parked=worker;
        runtime.current_thread=queue->second.thread;runtime.execute(runtime,c);runtime.current_thread=0;
        if(c.pc!=IopRuntime::sentinel || c.get(29)!=worker.context.get(29))throw std::runtime_error("registered RPC handler did not return to its worker frame");
        if(worker.wait_ee_bind || worker.wait_semaphore || worker.wait_sleep || worker.wait_mbx || worker.exited || !(worker.context==parked.context))
            throw std::runtime_error("registered RPC handler left its worker blocked");
        worker.ready=parked.ready;worker.waiting=parked.waiting;worker.wait_satisfied=parked.wait_satisfied;worker.wait_event=parked.wait_event;
        worker.wait_bits=parked.wait_bits;worker.wait_mode=parked.wait_mode;worker.wait_result=parked.wait_result;worker.wait_rpc=parked.wait_rpc;
        if(!c.get(2))throw std::runtime_error("registered RPC returned null response");
        runtime.check(IopRuntime::physical(c.get(2)),result_size);
        runtime.write(server.queue+4,0,4);
        return {runtime.ram.begin()+IopRuntime::physical(c.get(2)),runtime.ram.begin()+IopRuntime::physical(c.get(2))+result_size};
    } catch(...) {runtime.current_thread=0;failed_=true;throw;}
}

void NativeIopService::advanceVblank(unsigned epoch,std::span<std::uint8_t> ee_memory) {
    if(epoch!=epoch_ || failed_)return;
    runtime.pad.advanceField(); // Scripted input changes at field starts, before the pad threads poll.
    // Threads woken at the VBlank edges send to the EE: DBCMAN the pad state, CRI_ADXI its stream
    // buffers. Owned logical transport (copy at submission), scoped to the edges.
    const bool scoped=!runtime.providers.contains({"sifman",7}) && !runtime.providers.contains({"sifman",8}) && !runtime.providers.contains({"sifman",32}) && ee_memory.size()==32u*1024*1024;
    std::set<std::uint32_t> pending;
    if(scoped) {
        runtime.providers[{"sifman",7}]=[this,ee_memory,&pending](IopRuntime& rt,IopContext& c) {
            const auto descriptor=c.get(4);
            const auto thread=rt.threads.find(rt.current_thread);
            // DBCMAN: pad state to a registered socket buffer (see sceSifSetDmaIntr below).
            const bool dbcman=c.pc==0x621DC && c.get(28)==0x6A720;
            // CRI_ADXI's server thread returns a registered stream buffer to the EE (the reverse of the
            // EE->IOP stream copies: the same two buffer pairs registered by create commands 0/1).
            const bool cri=c.pc==0x1B3524 && c.get(28)==0x1BCDF0 && thread!=rt.threads.end() && thread->second.entry==0x1A7088 && cri_create_calls==2;
            if(c.get(5)!=1 || (!dbcman && !cri) || rt.in_interrupt) {
                std::string d;for(unsigned n=0;n<c.get(5) && n<4;++n)for(unsigned w=0;w<4;++w)d+=" "+std::to_string(rt.read(c.get(4)+n*16+w*4,4));
                throw std::runtime_error("Unsupported reverse DMA at VBlank: pc "+std::to_string(c.pc)+" count "+std::to_string(c.get(5))+" thread "+std::to_string(rt.current_thread)
                    +" entry "+std::to_string(thread!=rt.threads.end()?thread->second.entry:0)+" descriptors"+d);
            }
            rt.check(IopRuntime::physical(descriptor),16);
            const auto src=rt.read(descriptor,4),dst=rt.read(descriptor+4,4),size=rt.read(descriptor+8,4),attr=rt.read(descriptor+12,4);
            const bool cri_pair=(src==0x14FD40 && dst==0x365BC0) || (src==0x191740 && dst==0x35FD00);
            if(attr || (dbcman && (dst!=rt.read(0x62774,4) || size!=128)) || (cri && (!cri_pair || size!=0x880)) || dst>ee_memory.size()-size)
                throw std::runtime_error("Reverse DMA descriptor differs: src "+std::to_string(src)+" dst "+std::to_string(dst)+" size "+std::to_string(size));
            rt.check(IopRuntime::physical(src),size);
            std::copy_n(rt.ram.begin()+IopRuntime::physical(src),size,ee_memory.begin()+dst);
            const auto id=cri?0x740000u+(++cri_stream_returns&0xFFFF):0x720000u+(++dbc_pad_transfers&0xFFFF);
            pending.insert(id);return id;
        };
        runtime.providers[{"sifman",8}]=[&pending](IopRuntime&,IopContext& c) {
            // Owned logical transfers complete at submission: a status poll of a submitted ID reports done.
            if(!pending.contains(c.get(4)) || (c.pc!=0x621E4 && (c.pc<0x1A0000 || c.pc>=0x1BD000)))throw std::runtime_error("Unsupported reverse DMA completion poll");
            pending.erase(c.get(4));return UINT32_MAX;
        };
        // sceSifSetDmaIntr: the same transfer with a completion callback, run from the DMA completion
        // interrupt (logical: the copy completes at once, then the callback runs in interrupt context).
        runtime.providers[{"sifman",32}]=[this,ee_memory](IopRuntime& rt,IopContext& c) {
            const auto descriptor=c.get(4),handler=c.get(6),argument=c.get(7);
            if(c.pc!=0x621F4 || c.get(5)!=1 || c.get(28)!=0x6A720 || !handler || rt.in_interrupt || !rt.current_thread)
                throw std::runtime_error("Unsupported DBCMAN pad DMA (interrupt) submission");
            rt.check(IopRuntime::physical(descriptor),16);rt.check(IopRuntime::physical(handler),4);
            const auto src=rt.read(descriptor,4),dst=rt.read(descriptor+4,4),size=rt.read(descriptor+8,4),attr=rt.read(descriptor+12,4);
            // Destinations: the EE socket buffers the game registered with DBCMAN in its initialization
            // requests (slot n: 0x3B0B00+n*0x180 and +0x80, request words 10/11).
            const bool registered=dst>=0x3B0B00 && dst<0x3B0B00+2*0x180 && ((dst-0x3B0B00)%0x180==0 || (dst-0x3B0B00)%0x180==0x80);
            if(!registered || size!=128 || attr || dst>ee_memory.size()-size)
                throw std::runtime_error("DBCMAN pad DMA descriptor differs: src "+std::to_string(src)+" dst "+std::to_string(dst)+" size "+std::to_string(size)+" attr "+std::to_string(attr)+" retained "+std::to_string(rt.read(0x62774,4)));
            rt.check(IopRuntime::physical(src),size);
            std::copy_n(rt.ram.begin()+IopRuntime::physical(src),size,ee_memory.begin()+dst);
            ++dbc_pad_transfers;
            IopContext callback=c;callback.pc=handler;callback.branch_pending=false;callback.delay_instruction=false;callback.load_register=0;
            callback.set(4,argument);callback.set(28,0x6A720);callback.set(29,0x1FFE00);callback.set(31,IopRuntime::sentinel);
            const auto interrupts=rt.cpu_interrupts_enabled;rt.in_interrupt=true;rt.cpu_interrupts_enabled=false;
            try {rt.execute(rt,callback);} catch(...) {rt.in_interrupt=false;rt.cpu_interrupts_enabled=interrupts;throw;}
            rt.in_interrupt=false;rt.cpu_interrupts_enabled=interrupts;
            if(callback.pc!=IopRuntime::sentinel)throw std::runtime_error("DBCMAN DMA completion callback did not return");
            return 0x730000u+(dbc_pad_transfers&0xFFFF);
        };
    }
    auto release=[&]{if(scoped){runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});runtime.providers.erase({"sifman",32});}};
    try {runtime.advanceLogicalField();runtime.deliverVblankEdge(true);runtime.deliverVblankEdge(false);release();} catch(...) {release();failed_=true;throw;}
}
