#include "module_service.h"

// Memory card services (MCSERV/MCMAN): the RPC bind and card queries over the absent-card SIO2 profile.

std::vector<std::uint8_t> NativeIopService::callMcRpc(unsigned epoch,std::span<const std::uint8_t> input) {
    if(epoch!=epoch_ || failed_ || mc_rpc_requests || dbc_rpc_requests!=4 || input.size()!=48
        || std::any_of(input.begin(),input.end(),[](auto b){return b!=0;}))throw std::runtime_error("MCSERV outside observed initial request");
    auto response=executeRegisteredRpc(0x80000400,0xFE,input,12,0x3A6F80,0x3A8540);
    ++mc_rpc_requests;return response;
}

std::vector<std::uint8_t> NativeIopService::callMcQuery(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee_memory) {
    // GetInfo descriptor (public mcDescParam_t): port, slot, flags, then the type/free/format request
    // flags, buffer and the EE end-parameter address. Only port 0 / slot 0 has captured SIO2 evidence.
    std::array<std::uint32_t,12> words{};if(input.size()==48)std::memcpy(words.data(),input.data(),48);
    const bool first=!mc_query_requests;
    if(epoch!=epoch_ || failed_ || mc_rpc_requests!=1 || input.size()!=48 || words[0] || words[1] || words[2]
        || words[3]>1 || words[4]>1 || words[5]>1 || words[6] || words[7]!=0x3A7480 || words[8] || words[9] || words[10] || words[11]
        || (first && (words[3] || words[4] || words[5])))throw std::runtime_error("MCSERV query outside captured profile");
    if(ee_memory.size()!=32u*1024*1024 || (first && ee_sound_server_owned && runtime.providers.contains({"thbase",24})) || runtime.pending_ee_bind.thread || runtime.providers.contains({"sifcmd",15}) || runtime.providers.contains({"sifman",7}) || runtime.providers.contains({"sifman",8}))
        throw std::runtime_error("Card timed dispatch requires owned EE RPC endpoint");
    if(!first && (ee_sound_bind_returns!=1 || runtime.timed_wait_owner))
        throw std::runtime_error("Later MCSERV query before the sound bind completed");
    runtime.beginCardQuery();
    const auto dma_base=mc_dma_submissions,poll_base=mc_dma_polls;
    if(first)runtime.providers[{"sifcmd",15}]=[this,ee_memory](IopRuntime& rt,IopContext& c)->std::uint32_t {
        if(c.pc!=0xA1244 || c.get(4)!=0xA1CA0 || c.get(5)!=0x80000704 || c.get(6)
            || rt.pending_ee_bind.thread || !rt.timed_wait_owner || !rt.threads.contains(rt.current_thread)
            || rt.threads.at(rt.current_thread).entry!=0xA0FF0 || rt.threads.at(rt.current_thread).priority!=24)
            throw std::runtime_error("IOP EE bind outside reached SDRDRV helper");
        std::uint32_t registered;std::memcpy(&registered,ee_memory.data()+0x3A6D18,4);
        if(registered && !ee_sound_server_owned)throw std::runtime_error("EE bind lacks owned registration");
        constexpr unsigned packet=0x1FF400;
        rt.check(0xA1CA0,40);rt.check(packet,64);
        if(rt.read(0xA1CA0,4) || rt.read(0xA1CC4,4))throw std::runtime_error("Duplicate native EE sound bind");
        // Own the outgoing request before either parking it or completing against the registered EE server.
        const std::array<std::uint32_t,16> request={64,0,0x80000009,0,6,packet,2,0xA1CA0,0x80000704};
        for(unsigned n=0;n<request.size();++n)rt.write(packet+n*4,request[n],4);
        rt.write(0xA1CA0,packet,4);rt.write(0xA1CA4,2,4);rt.write(0xA1CA8,0,4);
        if(ee_sound_server_owned) {
            std::uint32_t server,handler,buffer;
            std::memcpy(&server,ee_memory.data()+0x3A6D18,4);
            std::memcpy(&handler,ee_memory.data()+0x3A6D1C,4);
            std::memcpy(&buffer,ee_memory.data()+0x3A6D20,4);
            if(server!=0x80000704 || handler!=0x3153F0 || buffer!=0x3A6CC0 || ee_sound_bind_returns)
                throw std::runtime_error("Owned EE registration changed before bind completion");
            rt.write(0xA1CA0,0,4);rt.write(0xA1CB4,buffer,4);rt.write(0xA1CC4,0x3A6D18,4);
            rt.write(packet+16,4,4);rt.write(packet+24,0,4);
            ++ee_sound_bind_returns;return 0;
        }
        rt.pending_ee_bind={rt.current_thread,0xA1CA0,0x80000704,packet,2};
        auto& t=rt.threads.at(rt.current_thread);t.ready=false;t.waiting=true;t.wait_ee_bind=true;
        throw IopYield{};
    };
    if(first && ee_sound_server_owned) {
        if(runtime.providers.contains({"thbase",24}))throw std::runtime_error("Sound sleep provider already owned");
        runtime.providers[{"thbase",24}]=[this](IopRuntime& rt,IopContext& c)->unsigned {
            if(!rt.threads.contains(rt.current_thread))throw std::runtime_error("Sound sleep lacks owned thread");
            auto& owner=rt.threads.at(rt.current_thread);
            if(ee_sound_bind_returns!=1 || c.pc!=0xA1304 || c.get(31)!=0xA0F24
                || owner.entry!=0xA0FF0 || owner.priority!=24 || c.get(28)!=owner.gp
                || owner.waiting || !owner.ready || rt.pending_ee_bind.thread
                || rt.read(0xA1CA0,4) || rt.read(0xA1CB4,4)!=0x3A6CC0 || rt.read(0xA1CC4,4)!=0x3A6D18)
                throw std::runtime_error("Queued sound bind reached unvalidated IOP sleep");
            owner.waiting=true;owner.ready=false;owner.wait_ee_bind=false;throw IopYield{};
        };
    }
    runtime.providers[{"sifman",7}]=[this,ee_memory,dma_base](IopRuntime& rt,IopContext& c) {
        if(c.pc!=0x51064 || c.get(4)!=0x1E1778 || c.get(5)!=1 || mc_dma_submissions!=dma_base
            || rt.cardWakes()!=5 || rt.cardAlarmFires()!=4 || rt.cardFifo()!=5)
            throw std::runtime_error("MCSERV secondary DMA outside owned query");
        const std::array<std::uint32_t,4> descriptor={0x1E16B8,0x3A7480,192,0};
        rt.check(c.get(4),16);rt.check(descriptor[0],descriptor[2]);
        for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+4*n,4)!=descriptor[n])
            throw std::runtime_error("MCSERV secondary DMA descriptor differs");
        std::copy_n(rt.ram.begin()+descriptor[0],192,mc_dma_payload.begin());
        std::copy(mc_dma_payload.begin(),mc_dma_payload.end(),ee_memory.begin()+descriptor[1]);
        ++mc_dma_submissions;return 0x730001u;
    };
    runtime.providers[{"sifman",8}]=[this,dma_base,poll_base](IopRuntime&,IopContext& c) {
        if(c.pc!=0x5106C || c.get(4)!=0x730001 || mc_dma_submissions!=dma_base+1 || mc_dma_polls!=poll_base)
            throw std::runtime_error("MCSERV secondary DMA poll differs");
        ++mc_dma_polls;return UINT32_MAX;
    };
    const auto cleanup=[&]{if(first)runtime.providers.erase({"sifcmd",15});runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});if(first && ee_sound_server_owned)runtime.providers.erase({"thbase",24});};
    std::vector<std::uint8_t> response;
    try {response=executeRegisteredRpc(0x80000400,1,input,4,0x3A6F80,0x3A8540);cleanup();}
    catch(...) {cleanup();throw;}
    if(mc_dma_submissions!=dma_base+1 || mc_dma_polls!=poll_base+1){failed_=true;throw std::runtime_error("MCSERV secondary DMA incomplete");}

    ++mc_query_requests;return response;
}
