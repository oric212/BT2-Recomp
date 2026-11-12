#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// BT2_REPLAY_CALL (docs/plans/validation/PCSX2_COMPARISON_PLAN.md): one guest function run on a state captured
// from PCSX2, to compare what it leaves with PCSX2's own state after the same call. The spec is
// "<RAM in>|<registers in>|<hex function>|<RAM out>[|<VU memory in>]". The registers are the block a checkpoint
// stub publishes (tools/pcsx2_compare/context_block.py has the layout); the VU memory is PCSX2's VUmem block
// (VU0 micro 4 KB, VU0 data 4 KB, VU1 micro 16 KB, VU1 data 16 KB), of which VU0's data memory and VU1's two
// memories are loaded. "|<hardware in>" (step 6, a VIF1 kick) adds the hardware words the stub read: VIF1's
// registers 0x10003C00 to 0x10003D70 and GIF_STAT (context_block.py, HARDWARE_WORDS), loaded into the VIF1 model.
// "|<VU1 registers in>" adds VU1's registers as PCSX2 keeps them (tools/pcsx2_compare/vu_registers.py: VF[32],
// VI[32] at 16 bytes each with the control registers among them, ACC); without it VU1's registers start at reset,
// though the game keeps values in them from one chain to the next. "|<GIF in>" (step 7) is GIF data sent to the GS
// before the call (tools/pcsx2_compare/gs_memory.py: PCSX2's GS registers and its whole GS memory as one upload);
// GS memory after the call is then written to <RAM out>.gs. The call returns to a sentinel; the RAM
// after it is written to <RAM out>, the registers, in the same layout, to <RAM out>.registers, and VU1's data
// memory to <RAM out>.vu1.
namespace {
// The register block's layout (context_block.py).
constexpr std::size_t gprs=0x000,fprs=0x200,hilo=0x280,sa=0x2A0,fcr31=0x2A4,clip=0x2B0,rand=0x2B4,q=0x2BC,vis=0x2C0,vfs=0x300,
    block_size=0x500,vu0_data=0x1000,vu1_program=0x2000,vu1_data=0x6000,vu_size=0xA000,hardware_size=25*4;

std::vector<char> readFile(const std::string& path,std::size_t size) {
    std::vector<char> data(size);
    std::ifstream in(path,std::ios::binary);
    if(!in.read(data.data(),std::streamsize(size)))throw std::runtime_error("BT2_REPLAY_CALL: cannot read "+path);
    return data;
}

template<class T> T field(const std::vector<char>& block,std::size_t offset) {T value;std::memcpy(&value,block.data()+offset,sizeof value);return value;}
template<class T> void put(std::vector<char>& block,std::size_t offset,const T& value) {std::memcpy(block.data()+offset,&value,sizeof value);}

// VIF1's registers as the stub read them (index k is 0x10003C00 + 0x10 * k; 24 is GIF_STAT).
void loadVif1(Vif1& vif1,const std::vector<char>& words) {
    const auto w=[&](unsigned k) {return field<std::uint32_t>(words,4*k);};
    auto& r=vif1.r;
    r.mark=w(3);r.cycle=w(4);r.mode=w(5);r.mask=w(7);r.code=w(8);r.itops=w(9);r.base=w(10);r.ofst=w(11);
    r.tops=w(12);r.itop=w(13);r.top=w(14);
    for(unsigned k=0;k<4;++k) {r.row[k]=w(16+k);r.col[k]=w(20+k);}
    r.dbf=(w(0)>>7)&1;      // VIF1_STAT.DBF.
    r.mskpath3=(w(24)>>1)&1; // GIF_STAT.M3P: PATH3 masked by VIF1's MSKPATH3.
    vif1.vu.top=r.top;vif1.vu.itop=r.itop;
}

// VU1's registers from PCSX2's VURegs entry (vu_registers.py: VF at 0, VI at 0x200, ACC at 0x400).
constexpr std::size_t vu1_registers_size=0x430;
void loadVu1Registers(Vu1& vu,const std::vector<char>& b) {
    for(unsigned k=0;k<32;++k)std::memcpy(vu.vf[k].data(),b.data()+16*k,16);
    const auto vi=[&](unsigned k) {return field<std::uint32_t>(b,0x200+16*k);};
    for(unsigned k=0;k<16;++k)vu.vi[k]=static_cast<std::uint16_t>(vi(k));
    vu.clip=vi(18)&0xFFFFFF;vu.i=vi(21);vu.q=vi(22);
    std::memcpy(vu.acc.data(),b.data()+0x400,16);
}

// R lives in the runtime (EeRuntime::vu0_random_, the exponent of 1.0 above its 23 bits), the block holds the 23
// bits as CFC2 reads them; R5900Context::vu0_r is the raw generated code's storage, unused once it is prepared.
void load(EeRuntime& runtime,R5900Context& c,const std::vector<char>& b) {
    for(int i=0;i<32;++i)std::memcpy(&c.r[i],b.data()+gprs+16*i,16);
    for(int i=0;i<32;++i)c.f[i]=field<float>(b,fprs+4*i);
    c.hi=field<std::uint64_t>(b,hilo);c.lo=field<std::uint64_t>(b,hilo+8);
    c.hi1=field<std::uint64_t>(b,hilo+16);c.lo1=field<std::uint64_t>(b,hilo+24);
    c.sa=field<std::uint32_t>(b,sa);c.fcr31=field<std::uint32_t>(b,fcr31);
    c.vu0_clip_flags=field<std::uint32_t>(b,clip);
    runtime.vu0_random_=0x3F800000u|(field<std::uint32_t>(b,rand)&0x7FFFFFu);c.vu0_q=field<float>(b,q);
    for(int i=0;i<16;++i)c.vi[i]=static_cast<std::uint16_t>(field<std::uint32_t>(b,vis+4*i));
    for(int i=0;i<32;++i)c.vu0_vf[i]=_mm_loadu_ps(reinterpret_cast<const float*>(b.data()+vfs+16*i));
}

std::vector<char> save(const EeRuntime& runtime,const R5900Context& c) {
    std::vector<char> b(block_size);
    for(int i=0;i<32;++i)std::memcpy(b.data()+gprs+16*i,&c.r[i],16);
    for(int i=0;i<32;++i)put(b,fprs+4*i,c.f[i]);
    put(b,hilo,c.hi);put(b,hilo+8,c.lo);put(b,hilo+16,c.hi1);put(b,hilo+24,c.lo1);
    put(b,sa,c.sa);put(b,fcr31,c.fcr31);put(b,clip,c.vu0_clip_flags);
    put(b,rand,runtime.vu0_random_&0x7FFFFFu);put(b,q,c.vu0_q);
    for(int i=0;i<16;++i)put(b,vis+4*i,std::uint32_t{c.vi[i]});
    for(int i=0;i<32;++i)_mm_storeu_ps(reinterpret_cast<float*>(b.data()+vfs+16*i),c.vu0_vf[i]);
    return b;
}
}

std::function<std::vector<std::uint32_t>()> probe_gs_memory;

void replayCall(EeRuntime& runtime,const R5900Context& prepared,const std::string& spec) {
    std::vector<std::string> part;
    for(std::size_t at=0;;) {
        const auto end=spec.find('|',at);
        part.push_back(spec.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(part.size()<4||part.size()>8)
        throw std::runtime_error("BT2_REPLAY_CALL is <RAM in>|<registers in>|<function>|<RAM out>[|<VU memory in>[|<hardware in>[|<VU1 registers in>[|<GIF in>]]]]");
    auto ram=runtime.memory();
    const auto image=readFile(part[0],ram.size());
    std::memcpy(ram.data(),image.data(),ram.size());
    auto& vu1=runtime.vif1.vu;
    if(part.size()>=5) {
        const auto vu=readFile(part[4],vu_size);
        std::memcpy(runtime.vu0_memory_.data(),vu.data()+vu0_data,runtime.vu0_memory_.size());
        std::memcpy(vu1.program.data(),vu.data()+vu1_program,vu1.program.size());++vu1.program_generation;
        std::memcpy(vu1.data.data(),vu.data()+vu1_data,vu1.data.size());
    }
    if(part.size()>=6)loadVif1(runtime.vif1,readFile(part[5],hardware_size));
    if(part.size()>=7)loadVu1Registers(vu1,readFile(part[6],vu1_registers_size));
    const bool gs=part.size()==8;
    if(gs) {
        if(!probe_gs_memory)throw std::runtime_error("BT2_REPLAY_CALL: GS memory needs the GPU renderer");
        const auto size=std::filesystem::file_size(part[7]);
        const auto packets=readFile(part[7],size);
        runtime.gs_sink.transfer(std::span(reinterpret_cast<const std::uint8_t*>(packets.data()),packets.size()));
    }
    constexpr std::uint32_t sentinel=0xFFFFFFFC;
    R5900Context call=prepared;
    load(runtime,call,readFile(part[1],block_size));
    call.r[31].low=sentinel;call.r[31].high=0;
    runtime.expected_return=sentinel;
    // Function 0 runs nothing: the state as loaded is written out (a check of the loading itself).
    const auto function=static_cast<std::uint32_t>(std::stoul(part[2],nullptr,16));
    if(function) {
        try {runtime.callFunction(function,call);}
        catch(const UnsupportedSyscall& stop) {
            // A boundary the replay does not cross (the kernel); named, so the comparison can list it as such.
            std::ostringstream text;
            text<<std::hex<<"BT2_REPLAY_CALL: stopped at system call 0x"<<stop.selector<<" at 0x"<<stop.resume_pc;
            throw std::runtime_error(text.str());
        }
        if(static_cast<std::uint32_t>(call.pc)!=sentinel)throw std::runtime_error("BT2_REPLAY_CALL: the function did not return to the sentinel");
    }
    std::ofstream out(part[3],std::ios::binary);
    out.write(reinterpret_cast<const char*>(ram.data()),std::streamsize(ram.size()));
    const auto registers=save(runtime,call);
    std::ofstream(part[3]+".registers",std::ios::binary).write(registers.data(),std::streamsize(registers.size()));
    std::ofstream(part[3]+".vu1",std::ios::binary).write(reinterpret_cast<const char*>(vu1.data.data()),std::streamsize(vu1.data.size()));
    if(gs) {
        const auto memory=probe_gs_memory();
        std::ofstream(part[3]+".gs",std::ios::binary).write(reinterpret_cast<const char*>(memory.data()),std::streamsize(memory.size()*4));
    }
    std::cout<<std::hex<<"Replayed call: v0 "<<static_cast<std::uint32_t>(call.r[2].low)<<std::dec<<" VU1 runs "<<vu1.runs
             <<" kicks "<<vu1.kicks<<std::endl;
}
