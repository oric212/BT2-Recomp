#include "module_service.h"

// Controller services (DBCMAN): startup RPCs and the frame-phase main/port requests.

std::vector<std::uint8_t> NativeIopService::callDbcRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::span<std::uint8_t> ee_memory) {
    if(epoch!=epoch_ || failed_ || dbc_rpc_requests>=4 || command!=(dbc_rpc_requests>=2?0x80001301u:dbc_rpc_requests?0x80001304u:0x80001363u) || input.size()!=144 || result_size!=144
        || sounds_rpc_requests!=1 || residents.size()!=13 || boot_heap_allocations!=2)throw std::runtime_error("Unsupported DBCMAN native request");
    std::array<std::uint32_t,36> expected{};if(dbc_rpc_requests==1){expected[0]=0x310;expected[1]=0x3A27C0;}
    if(dbc_rpc_requests>=2){expected[0]=3;expected[1]=1;expected[2]=dbc_rpc_requests-2;expected[10]=0x3B0B00+(dbc_rpc_requests-2)*0x180;expected[11]=expected[10]+0x80;}
    if(std::memcmp(input.data(),expected.data(),sizeof(expected)))throw std::runtime_error("DBCMAN input outside observed initialization profile");
    const bool transfer=dbc_rpc_requests==1 && !ee_memory.empty();
    if(transfer) {
        if(ee_memory.size()!=32u*1024*1024 || dbc_dma_submissions || dbc_dma_polls || runtime.providers.contains({"sifman",7}) || runtime.providers.contains({"sifman",8}))
            throw std::runtime_error("DBCMAN DMA endpoint outside initial owned scope");
            runtime.providers[{"sifman",7}]=[this,ee_memory](IopRuntime& rt,IopContext& c) {
            const auto descriptor=c.get(4);
            if(c.pc!=0x621DC || c.get(5)!=1 || c.get(28)!=0x6A720 || descriptor!=0x1E1EE0 || dbc_dma_submissions)
                throw std::runtime_error("Unsupported DBCMAN reverse DMA submission");
            rt.check(descriptor,16);
            const std::array<std::uint32_t,4> expected={0x62778,0x3A27C0,128,0};
            for(unsigned n=0;n<4;++n)if(rt.read(descriptor+n*4,4)!=expected[n])throw std::runtime_error("DBCMAN DMA descriptor differs");
            rt.check(expected[0],expected[2]);
            if(expected[1]>ee_memory.size()-expected[2])throw std::runtime_error("DBCMAN DMA EE destination outside RAM");
            // Owned logical transport; bytes come from actual native IOP state.
            std::copy_n(rt.ram.begin()+expected[0],expected[2],dbc_dma_payload.begin());
            std::copy(dbc_dma_payload.begin(),dbc_dma_payload.end(),ee_memory.begin()+expected[1]);
            std::copy_n(ee_memory.begin()+expected[1],dbc_dma_ee_snapshot.size(),dbc_dma_ee_snapshot.begin());
            ++dbc_dma_submissions;return 0x710001u;
        };
        runtime.providers[{"sifman",8}]=[this](IopRuntime&,IopContext& c) {
            if(c.pc!=0x621E4 || c.get(4)!=0x710001 || dbc_dma_submissions!=1 || dbc_dma_polls)
                throw std::runtime_error("Unsupported DBCMAN reverse DMA completion poll");
            ++dbc_dma_polls;return UINT32_MAX;
        };
    }
    try {
        auto response=executeRegisteredRpc(0x80001300,command,input,result_size,0x3A03C0,0x3A04C0);
        if(transfer && (dbc_dma_submissions!=1 || dbc_dma_polls!=1))throw std::runtime_error("DBCMAN DMA lifecycle incomplete");
        if(transfer){runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});}
        ++dbc_rpc_requests;return response;
    }catch(...) {if(transfer){runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});}throw;}

}

std::vector<std::uint8_t> NativeIopService::callDbcMain(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input) {
    if(epoch!=epoch_ || failed_ || dbc_rpc_requests!=4 || (command&0xFFFFFF00u)!=0x80001300u || input.size()!=144)throw std::runtime_error("Unsupported DBCMAN frame request");
    auto response=executeRegisteredRpc(0x80001300,command,input,144,0x3A03C0,0x3A04C0);
    ++dbc_main_requests;return response;
}

std::vector<std::uint8_t> NativeIopService::callDbcPort(unsigned epoch,std::uint32_t service,std::uint32_t client,std::uint32_t result,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size) {
    if(epoch!=epoch_ || failed_ || dbc_rpc_requests!=4 || (service!=0x8000131E && service!=0x8000131F) || command>1
        || input.size()!=144 || result_size!=144)
        throw std::runtime_error("Unsupported DBC port request");
    auto response=executeRegisteredRpc(service,command,input,result_size,client,result);
    ++dbc_port_requests;return response;
}
