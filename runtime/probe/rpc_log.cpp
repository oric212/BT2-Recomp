#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// BT2_RPC_LOG=<file> (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 8): the port's side of a comparison
// session's service log (tools/pcsx2_compare/rpc_log.py). The game's sceSifCallRpc (0x332970) runs here too, its
// requests answered by the native services, so the same two points are logged: each call's entry (the request)
// and, for a call that waited, the call it makes from 0x332B34 right after its semaphore wait (the reply in its
// receive buffer). One line per record: Q (request) or R (reply), the function number, the client, the mode, the
// buffer's size, its address and the checksum of its whole words as the PCSX2 stub computes it (each word added,
// the sum rotated left by one), then the field (VBlanks delivered). Each bind (sceSifBindRpc, 0x332790) is a B line
// with the service's ID in the function's place, which names the client's service. Another guest-call hook
// installed before keeps running.
namespace {
constexpr std::uint32_t call_rpc=0x332970,after_wait=0x332B34,bind_rpc=0x332790;

std::uint32_t checksum(EeRuntime& runtime,std::uint32_t address,std::uint32_t size) {
    const auto ram=runtime.memory();
    const auto start=address&0x1FFFFFFF; // kseg and uncached aliases of RAM
    if(address>>28==7)throw std::runtime_error("BT2_RPC_LOG: an RPC buffer in the scratchpad");
    if(start+(size&~3u)>ram.size())throw std::runtime_error("BT2_RPC_LOG: an RPC buffer outside EE RAM");
    std::uint32_t total=0;
    for(std::uint32_t at=0;at+4<=size;at+=4) {
        std::uint32_t word;std::memcpy(&word,ram.data()+start+at,4);
        total+=word;total=(total<<1)|(total>>31);
    }
    return total;
}
}

void installRpcLog(EeRuntime& runtime,const std::string& file) {
    auto out=std::make_shared<std::ofstream>(file);
    if(!*out)throw std::runtime_error("Cannot write the RPC log "+file);
    auto previous=std::move(runtime.on_guest_call);
    runtime.on_guest_call=[&runtime,out,previous](const R5900Context& context,std::uint32_t from,std::uint32_t target) {
        if(previous)previous(context,from,target);
        const auto reg=[&](unsigned r){return static_cast<std::uint32_t>(context.r[r].low);};
        std::uint32_t kind,function,client,mode,size,address;
        if(target==call_rpc) {
            kind='Q';function=reg(5);client=reg(4);mode=reg(6);size=reg(8);address=reg(7);
        } else if(target==bind_rpc) {
            *out<<"B "<<std::hex<<reg(5)<<' '<<reg(4)<<' '<<reg(6)<<" 0 0 0"<<std::dec<<' '<<runtime.vblank_deliveries<<'\n';
            return;
        } else if(from==after_wait) {
            std::uint32_t receive_size;std::memcpy(&receive_size,runtime.memory().data()+(reg(16)&0x1FFFFFFF)+44,4);
            kind='R';function=reg(22);client=reg(17);mode=reg(30);size=receive_size;address=reg(20);
        } else return;
        *out<<static_cast<char>(kind)<<std::hex<<' '<<function<<' '<<client<<' '<<mode<<' '<<size<<' '<<address<<' '
            <<checksum(runtime,address,size)<<std::dec<<' '<<runtime.vblank_deliveries<<'\n';
    };
}
