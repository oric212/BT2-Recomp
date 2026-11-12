#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// BT2_REPLAY_CALL (docs/plans/validation/PCSX2_COMPARISON_PLAN.md): one guest function run on a state captured
// from PCSX2, to compare what it leaves with PCSX2's own state after the same call. The spec is
// "<RAM in>|<registers in>|<hex function>|<RAM out>[|<VU memory in>]". The registers are the block a checkpoint
// stub publishes (tools/pcsx2_compare/context_block.py has the layout); the VU memory is PCSX2's VUmem block
// (VU0 micro 4 KB, VU0 data 4 KB, VU1 micro 16 KB, VU1 data 16 KB), of which VU0's data memory is loaded. The call
// returns to a sentinel; the RAM after it is written to <RAM out> and the registers, in the same layout, to
// <RAM out>.registers.
namespace {
// The register block's layout (context_block.py).
constexpr std::size_t gprs=0x000,fprs=0x200,hilo=0x280,sa=0x2A0,fcr31=0x2A4,clip=0x2B0,rand=0x2B4,q=0x2BC,vis=0x2C0,vfs=0x300,
    block_size=0x500,vu0_data=0x1000;

std::vector<char> readFile(const std::string& path,std::size_t size) {
    std::vector<char> data(size);
    std::ifstream in(path,std::ios::binary);
    if(!in.read(data.data(),std::streamsize(size)))throw std::runtime_error("BT2_REPLAY_CALL: cannot read "+path);
    return data;
}

template<class T> T field(const std::vector<char>& block,std::size_t offset) {T value;std::memcpy(&value,block.data()+offset,sizeof value);return value;}
template<class T> void put(std::vector<char>& block,std::size_t offset,const T& value) {std::memcpy(block.data()+offset,&value,sizeof value);}

void load(R5900Context& c,const std::vector<char>& b) {
    for(int i=0;i<32;++i)std::memcpy(&c.r[i],b.data()+gprs+16*i,16);
    for(int i=0;i<32;++i)c.f[i]=field<float>(b,fprs+4*i);
    c.hi=field<std::uint64_t>(b,hilo);c.lo=field<std::uint64_t>(b,hilo+8);
    c.hi1=field<std::uint64_t>(b,hilo+16);c.lo1=field<std::uint64_t>(b,hilo+24);
    c.sa=field<std::uint32_t>(b,sa);c.fcr31=field<std::uint32_t>(b,fcr31);
    c.vu0_clip_flags=field<std::uint32_t>(b,clip);
    c.vu0_r=_mm_set_ss(field<float>(b,rand));c.vu0_q=field<float>(b,q);
    for(int i=0;i<16;++i)c.vi[i]=static_cast<std::uint16_t>(field<std::uint32_t>(b,vis+4*i));
    for(int i=0;i<32;++i)c.vu0_vf[i]=_mm_loadu_ps(reinterpret_cast<const float*>(b.data()+vfs+16*i));
}

std::vector<char> save(const R5900Context& c) {
    std::vector<char> b(block_size);
    for(int i=0;i<32;++i)std::memcpy(b.data()+gprs+16*i,&c.r[i],16);
    for(int i=0;i<32;++i)put(b,fprs+4*i,c.f[i]);
    put(b,hilo,c.hi);put(b,hilo+8,c.lo);put(b,hilo+16,c.hi1);put(b,hilo+24,c.lo1);
    put(b,sa,c.sa);put(b,fcr31,c.fcr31);put(b,clip,c.vu0_clip_flags);
    put(b,rand,_mm_cvtss_f32(c.vu0_r));put(b,q,c.vu0_q);
    for(int i=0;i<16;++i)put(b,vis+4*i,std::uint32_t{c.vi[i]});
    for(int i=0;i<32;++i)_mm_storeu_ps(reinterpret_cast<float*>(b.data()+vfs+16*i),c.vu0_vf[i]);
    return b;
}
}

void replayCall(EeRuntime& runtime,const R5900Context& prepared,const std::string& spec) {
    std::vector<std::string> part;
    for(std::size_t at=0;;) {
        const auto end=spec.find('|',at);
        part.push_back(spec.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(part.size()!=4&&part.size()!=5)
        throw std::runtime_error("BT2_REPLAY_CALL is <RAM in>|<registers in>|<function>|<RAM out>[|<VU memory in>]");
    auto ram=runtime.memory();
    const auto image=readFile(part[0],ram.size());
    std::memcpy(ram.data(),image.data(),ram.size());
    if(part.size()==5) {
        const auto vu=readFile(part[4],vu0_data+runtime.vu0_memory_.size());
        std::memcpy(runtime.vu0_memory_.data(),vu.data()+vu0_data,runtime.vu0_memory_.size());
    }
    constexpr std::uint32_t sentinel=0xFFFFFFFC;
    R5900Context call=prepared;
    load(call,readFile(part[1],block_size));
    call.r[31].low=sentinel;call.r[31].high=0;
    runtime.expected_return=sentinel;
    runtime.callFunction(static_cast<std::uint32_t>(std::stoul(part[2],nullptr,16)),call);
    if(static_cast<std::uint32_t>(call.pc)!=sentinel)throw std::runtime_error("BT2_REPLAY_CALL: the function did not return to the sentinel");
    std::ofstream out(part[3],std::ios::binary);
    out.write(reinterpret_cast<const char*>(ram.data()),std::streamsize(ram.size()));
    const auto registers=save(call);
    std::ofstream(part[3]+".registers",std::ios::binary).write(registers.data(),std::streamsize(registers.size()));
    std::cout<<std::hex<<"Replayed call: v0 "<<static_cast<std::uint32_t>(call.r[2].low)<<std::dec<<std::endl;
}
