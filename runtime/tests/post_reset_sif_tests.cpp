#include "ee/ee_runtime.h"
#include <iostream>
#include <vector>
void check(bool v) {if(!v)throw std::runtime_error("Fresh SIF assertion failed");}
template<class F> void reject(F f) {bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}check(caught);}
int main() {
    try {
    // The general SIF (services/sif_system.cpp) gives the original's handshake after the IOP reset (the
    // captured words of PostResetSif::evidence): D_STAT and SIF0's CHCR before and after the EE sets SIF0
    // up, SMFLG and SMCOM once IOPRP300 is up, the INIT_CMD packets and the IOP's SET_SREG reply.
    const auto& e=PostResetSif::evidence;
    EeRuntime rt;rt.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));auto* ram=rt.memory().data();
    rt.supplyDmaStatus(0x800040);rt.supplyDma5Control(0x184);rt.supplySifCommand(0x19600);
    R5900Context c{};c.pc=0x123450;c.r[29].low=0x100000;
    const auto call=[&](std::uint32_t selector,std::uint32_t a0,std::uint32_t a1=0,std::uint32_t a2=0) {
        c.r[3].low=selector;c.r[4].low=a0;c.r[5].low=a1;c.r[6].low=a2;rt.handleSyscall(ram,&c,0);return static_cast<std::uint32_t>(c.r[2].low);
    };
    // A GIF transfer completed before (CIS2); the IOP reboot to IOPRP300 stops SIF0's receive.
    rt.write64(ram,0x1000,0x1000000000008001ULL);rt.write64(ram,0x1008,0xE);rt.write64(ram,0x1010,0);rt.write64(ram,0x1018,0x49); // A+D: PABE.
    rt.Store32(ram,&c,0x1000A020,2);rt.Store32(ram,&c,0x1000A010,0x1000);rt.Store32(ram,&c,0x1000A000,0x101);
    const char image[]="rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
    const std::array<std::uint32_t,6> reset={104,0,0x80000003,0,36,0};
    for(unsigned n=0;n<6;++n)rt.write32(ram,0x2000+n*4,reset[n]);
    std::copy(std::begin(image),std::end(image),rt.memory().begin()+0x2018);
    const std::array<std::uint32_t,4> reset_dma={0x2000,0x19600,104,0x44};
    for(unsigned n=0;n<4;++n)rt.write32(ram,0x3000+n*4,reset_dma[n]);
    check(call(0x79,4,0x40000)==0 && call(0x77,0x3000,1)!=0 && rt.iop_epoch==1); // SMFLG cleared, then the reboot.
    check(call(0x79,4,0x10000)==0 && call(0x79,0x80000002,0)==0 && call(0x79,0x80000000,0)==0);
    check(call(0x7A,4)==0x70000 && rt.iop_ready_epoch==1); // The reboot finished when the EE first polls.
    check(rt.Load32(ram,&c,0x1000E010)==e[3] && rt.Load32(ram,&c,0x1000C000)==e[4]);
    call(0x78,0);check(rt.Load32(ram,&c,0x1000C000)==e[13]);
    check(call(0x12,5,0x331DF0)==1);check(call(0x16,5)==1 && rt.Load32(ram,&c,0x1000E010)==e[12]);
    check(call(0x7A,0x80000000)==0 && call(0x7A,4)==e[6] && call(0x7A,2)==e[8]);
    call(0x79,0x80000000,e[8]);call(0x79,0x80000001,0x3A9598);
    // INIT_CMD 0 (the EE's receive buffer), then INIT_CMD 1 (RPC set up), into IOPRP300's SIFCMD buffer.
    const std::array<std::uint32_t,5> init0={20,0,0x80000002,0,0x3A94C0};
    for(unsigned n=0;n<5;++n)rt.write32(ram,0x4000+n*4,init0[n]);
    const std::array<std::uint32_t,4> init_dma={0x4000,0x19600,20,0x44};
    for(unsigned n=0;n<4;++n)rt.write32(ram,0x5000+n*4,init_dma[n]);
    reject([&]{call(0x77,0x5000,1);}); // The BIOS's buffer is gone after the reboot.
    rt.write32(ram,0x5004,e[8]);check(call(0x77,0x5000,1)!=0 && rt.sif_ee_receive_address==0x3A94C0);
    check(call(0x7A,0x80000002)==0);
    rt.write32(ram,0x4000,16);rt.write32(ram,0x400C,1);rt.write32(ram,0x5008,16);check(call(0x77,0x5000,1)!=0);
    for(unsigned n=0;n<6;++n)check(rt.read32(ram,0x3A94C0+n*4)==e[26+n]);
    check(rt.read32(ram,0x3A9740)==1 && rt.post_sif.sreg_responses==1 && rt.post_sif.submissions==2);
    call(0x79,0x80000002,1);check(rt.post_sif.rpc_initialized);
    reject([&]{call(0x7A,3);}); // MSFLG is not reached.
    // DMAC handlers by id: removal frees the slot; the lowest free id is taken again.
    check(call(0x13,5,1)==0);reject([&]{call(0x13,5,1);});check(call(0x12,5,0x331DF0)==1);
    PostResetSif sif;
    auto evidence=std::span(reinterpret_cast<const std::uint8_t*>(e.data()),sizeof(e));
    reject([&]{sif.supply(evidence.first(64));});sif.supply(evidence);check(sif.supplied);reject([&]{sif.supply(evidence);});
    PostResetCd cd;
    std::array<std::uint32_t,52> replies{};
    for(unsigned n : {0u,16u}) {replies[n+2]=0x80000008;replies[n+5]=0x203A97C0;replies[n+7]=0x3A87A8;replies[n+8]=0x80000009;}
    replies[25]=0x43388;replies[26]=0x441B8;
    replies[32]=0x1000;replies[33]=0x3695C0;replies[34]=0x80000008;replies[39]=0x3A87A8;replies[40]=0x8000000A;
    replies[48]=1;replies[49]=replies[50]=0x225;
    auto service=std::span(reinterpret_cast<const std::uint8_t*>(replies.data()),sizeof(replies));
    reject([&]{cd.supply(service.first(204));});check(!cd.supplied);
    replies[25]=0x3F570;reject([&]{cd.supply(service);});check(!cd.supplied);
    replies[25]=0x43388;
    std::array<std::uint8_t,340> missing_service{};std::copy(service.begin(),service.end(),missing_service.begin());
    reject([&]{cd.supply(missing_service);});check(!cd.supplied);
    cd.supply(service);check(cd.supplied && !cd.scmd_supplied && !cd.operations());
    reject([&]{cd.supply(service);});
    PostResetLoadfile loader;
    std::array<std::uint32_t,33> lf{};
    lf[2]=0x80000008;lf[5]=0x203A97C0;lf[7]=0x3ACC00;lf[8]=0x80000009;lf[9]=0x22568;lf[10]=0x225B0;
    lf[16]=0x400;lf[17]=0x3ACA00;lf[18]=0x80000008;lf[23]=0x3ACC00;lf[24]=0x8000000A;lf[32]=0x30303033;
    auto lb=std::span(reinterpret_cast<const std::uint8_t*>(lf.data()),sizeof(lf));
    reject([&]{loader.supply(lb.first(128));});check(!loader.supplied);
    lf[9]=0x43388;reject([&]{loader.supply(lb);});check(!loader.supplied);
    lf[9]=0x22568;lf[32]=0;reject([&]{loader.supply(lb);});check(!loader.supplied);
    lf[32]=0x30303033;loader.supply(lb);check(loader.supplied && !loader.operations());
    reject([&]{loader.supply(lb);});
    std::cout<<"Fresh SIF transport tests passed\n";
    return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
