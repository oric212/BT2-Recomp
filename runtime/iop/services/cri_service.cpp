#include "module_service.h"
#include "cri_adxi_native.h"

// CRI ADX streaming service (CRI_ADXI): stream creation and control requests.

std::vector<std::uint8_t> NativeIopService::callCriCreate(unsigned epoch,std::span<const std::uint8_t> input) {
    const bool second=cri_create_calls==1 && cri_control_calls==34 && boot_heap_allocations==5;
    if(epoch!=epoch_ || failed_ || (cri_create_calls && !second) || input.size()!=16 || disc_search_calls!=6 || disc_read_calls!=1)
        throw std::runtime_error("CRI create outside observed lifecycle");
    const std::array<unsigned,4> expected=second?std::array<unsigned,4>{1,0x35FD00,0x191740,0x880}:std::array<unsigned,4>{0,0x365BC0,0x14FD40,0x880};
    for(unsigned n=0;n<4;++n){unsigned word;std::memcpy(&word,input.data()+n*4,4);if(word!=expected[n])throw std::runtime_error("CRI create input differs");}
    std::vector<std::uint8_t> response;
    try {response=executeRegisteredRpc(0x90000200,2,input,4,0x355C70,0x39DD80);}
    catch(const std::exception& e){failed_=true;throw std::runtime_error(std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc));}
    unsigned slot;std::memcpy(&slot,response.data(),4);
    if(slot!=(second?0x1B5904u:0x1B58C0u))throw std::runtime_error("CRI create did not return its owned slot");
    ++cri_create_calls;return response;
}

std::vector<std::uint8_t> NativeIopService::callCriControl(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input) {
    const bool channel=cri_control_calls>=34;
    const bool channel_setup=cri_control_calls==34;
    const bool batch=cri_control_calls>=2 && !channel;
    const bool stream=batch && !(cri_control_calls&1);
    const auto index=channel?(channel_setup?0u:cri_control_calls-35):batch?(cri_control_calls-2)/2:0u;
    const auto expected_command=channel?(channel_setup?0x40Au:0x408u):batch?(stream?0x422u:0x400u):cri_control_calls?0x40Cu:0x403u;
    const auto size=channel?(channel_setup?12u:16u):batch?(stream?12u:16u):cri_control_calls?12u:0u;
    // Startup verified five stream-control pairs (indices 0-4); later pairs (movie playback) continue
    // the same indexed objects and must still match the input pattern and the original return.
    if(epoch!=epoch_ || failed_ || cri_create_calls!=(channel?2u:1u) || (!channel && cri_control_calls>39) || (channel && index>=64)
        || command!=expected_command || input.size()!=size)
        throw std::runtime_error("CRI control outside observed lifecycle");
    if(command==0x40C) {
        const std::array<unsigned,3> expected={0xBB80,0xFFFF,0xFFFF};
        for(unsigned n=0;n<3;++n) {unsigned word;std::memcpy(&word,input.data()+n*4,4);
            if(word!=expected[n])throw std::runtime_error("CRI 0x40C input differs");}
    }
    if(batch || channel) {
        // Stream pairs: the channel buffer (checked below) stays allocated while later allocations happen.
        if((channel?boot_heap_allocations<6u:boot_heap_allocations!=4u) || !runtime.spu2_auto_dma.pending)throw std::runtime_error("CRI group lacks owned buffer");
        const std::array<unsigned,4> expected=channel?(channel_setup?std::array<unsigned,4>{0x10,0x192000,0x4040,0}
            :std::array<unsigned,4>{2,0,iop_cri_adxi_base+0x14A40+index*0x60,iop_cri_adxi_base+0x14A70+index*0x60})
            :stream?std::array<unsigned,4>{1,0x150640+index*0x4100,0x100,0}
            :std::array<unsigned,4>{0x364B08+index*0x30,iop_cri_adxi_base+0x14A40+index*0x30,1,0x365900+index*0x14};
        for(unsigned n=0;n<size/4;++n) {unsigned word;std::memcpy(&word,input.data()+n*4,4);
            if(word!=expected[n])throw std::runtime_error("CRI group input differs");}
        if(channel)runtime.check(0x192000,0x4040);else runtime.check(0x150640+index*0x4100,0x4100);
    }
    try {
        auto response=executeRegisteredRpc(0x90000200,command,input,channel_setup?0u:batch || channel?4u:command==0x40C?12u:0u,0x355C70,0x39DD80);
        if(batch || (channel && !channel_setup)) {
            unsigned result=0;std::memcpy(&result,response.data(),4);
            const auto expected=iop_cri_adxi_base+(channel?0xEDA0+index*0x1C:stream?0x14A40+index*0x30:0x15580+index*0x14);
            if(result!=expected)throw std::runtime_error("CRI group did not return owned original object");
        }
        ++cri_control_calls;return response;
    }catch(...){failed_=true;throw;}
}
