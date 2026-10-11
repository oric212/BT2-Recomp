#include <vector>
#include <cstring>
#include "ee/ipu_device.h"
#include "ee/stopped_dma_channels.h"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {
void check(bool value,std::source_location where=std::source_location::current()) {
    if(!value) {std::printf("IPU test failed at line %u\n",unsigned(where.line()));std::exit(1);}
}
template<class F> bool refused(F f) {try {f();} catch(const std::runtime_error&) {return true;}return false;}
}

int main() {
    // sceIpuInit: reset, BCLR, two SETIQ tables from eight FIFO quadwords, SETVQ, SETTH, reset, BCLR.
    IpuDevice ipu;
    check(ipu.write32(0x10002010,0x40000000) && ipu.read32(0x10002010)==0u);
    check(ipu.write32(0x10002000,0));
    for(unsigned q=0;q<8;++q)check(ipu.writeFifo(0x10007010,{q*4,q*4+1,q*4+2,q*4+3}));
    check(ipu.read32(0x10002010)==8u); // IFC counts the queued quadwords; BUSY stays clear.
    check(refused([&]{ipu.writeFifo(0x10007010,{});}));  // The FIFO holds eight quadwords.
    check(ipu.write32(0x10002000,0x50000000) && ipu.iq_loaded[0] && ipu.intra_iq[4]==1);
    check(ipu.write32(0x10002000,0x58000000) && ipu.iq_loaded[1] && ipu.nonintra_iq[0]==16 && ipu.read32(0x10002010)==0u);
    // SETVQ without its two quadwords waits with BUSY set and completes when they arrive.
    check(ipu.write32(0x10002000,0x60000000) && ipu.busy() && (*ipu.read32(0x10002010)&0x80000000u));
    ipu.writeFifo(0x10007010,{0x00020001,0,0,0});ipu.writeFifo(0x10007010,{0,0,0,0x80000000});
    check(!ipu.busy() && ipu.vq_loaded && ipu.vq_clut[0]==1 && ipu.vq_clut[1]==2 && ipu.vq_clut[15]==0x8000);
    check(ipu.write32(0x10002000,0x90000000) && ipu.thresholds==0);
    check(ipu.write32(0x10002010,0x40000000) && ipu.resets==2 && ipu.commands==5);
    // Decode parameters (MP1 here) are retained and read back; other control bits are refused.
    check(ipu.write32(0x10002010,0x00800000) && ipu.read32(0x10002010)==0x00800000u);
    check(refused([&]{ipu.write32(0x10002010,0x00000100);}));
    // Decode commands and IPU DMA starts are refused; stopped channel registers are retained.
    check(refused([&]{ipu.write32(0x10002000,0x70000000);}));
    check(refused([&]{ipu.write32(0x1000B400,0x100);})); // toIPU must read from memory (DIR 1).
    {
        // toIPU source chain feeding the input on demand: REF to 0x200 (1 qword), then END with one
        // qword after the tag. FDEC skips FB bits and returns the next 32 (MSB first, memory byte order).
        IpuDevice dev;std::vector<std::uint8_t> ram(0x400);
        auto tag=[&](std::uint32_t at,std::uint64_t lo){std::memcpy(ram.data()+at,&lo,8);};
        tag(0x100,1|(3ull<<28)|(0x200ull<<32));tag(0x110,1|(7ull<<28));
        const std::uint8_t first[16]={0x00,0x00,0x01,0xB3,0x20,0x01,0xC0,0x24};std::memcpy(ram.data()+0x200,first,16);
        ram[0x120]=0xAB;dev.ram=ram;
        check(dev.write32(0x1000B430,0x100) && dev.write32(0x1000B420,0) && dev.write32(0x1000B400,0x105));
        check(dev.write32(0x10002000,0x40000000) && *dev.read32(0x10002000)==0x000001B3u); // FDEC 0.
        check(!(*dev.read32(0x1000B400)&0x100) && *dev.read32(0x1000B430)==0x130);         // The start filled the FIFO: both tags done.
        check(dev.write32(0x10002000,0x40000020) && *dev.read32(0x10002000)==0x2001C024u); // FDEC 32.
        check(*dev.read32(0x10002030)==0x2001C024u && (*dev.read32(0x10002020)&0x7F)==32);
        // FDEC 96 + 32 bits reach into the END quadword.
        check(dev.write32(0x10002000,0x40000020) && dev.write32(0x10002000,0x40000020) && dev.write32(0x10002000,0x40000020));
        check(*dev.read32(0x10002000)==0xAB000000u);
        {
            // Clearing STR stops a running chain; a restart continues the interrupted tag's data.
            IpuDevice stop;std::vector<std::uint8_t> mem(0x400);
            auto put=[&](std::uint32_t at,std::uint64_t lo){std::memcpy(mem.data()+at,&lo,8);};
            // REF of 12 quadwords: the start fills the FIFO (8) and stops there; 4 remain in the tag.
            put(0x100,12|(3ull<<28)|(0x200ull<<32));put(0x110,1|(7ull<<28));mem[0x280]=0x77;stop.ram=mem;
            check(stop.write32(0x1000B430,0x100) && stop.write32(0x1000B400,0x105) && *stop.read32(0x1000B420)==4);
            check(((*stop.read32(0x10002020)>>8)&15)==8);
            check(stop.write32(0x1000B400,0x5) && !(*stop.read32(0x1000B400)&0x100) && *stop.read32(0x1000B420)==4);
            // BCLR empties the FIFO; the restart continues the interrupted tag at quadword 8 (0x280).
            check(stop.write32(0x10002000,0x00000000) && stop.write32(0x1000B400,0x105) && stop.write32(0x10002000,0x40000000));
            check(*stop.read32(0x10002000)==0x77000000u);
        }
        // Past the end of the input the command waits; a new transfer completes it.
        // (FB is 6 bits: two FDEC 63 need 63+63+32 bits, 30 more than the 128 left.)
        check(dev.write32(0x10002000,0x4000003F) && !dev.busy() && dev.write32(0x10002000,0x4000003F) && dev.busy());
        tag(0x130,1|(7ull<<28));ram[0x140]=0xCD;dev.ram=ram;
        check(dev.write32(0x1000B430,0x130) && dev.write32(0x1000B400,0x105) && !dev.busy() && *dev.read32(0x10002000)==(0xCDu<<22));
    }
    check(ipu.write32(0x1000B020,0x12345) && ipu.read32(0x1000B020)==0x2345u && !ipu.read32(0x10003000));
    // Stopped scratchpad channels: registers retained (QWC 16 bits, SADR within 16 KB), STR refused.
    StoppedDmaChannels spr{0x1000D000,0x1000D400};
    check(spr.write32(0x1000D400,0) && spr.write32(0x1000D420,0x12345) && spr.read32(0x1000D420)==0x2345u);
    check(spr.write32(0x1000D480,0x7FFF) && spr.read32(0x1000D480)==0x3FF0u && !spr.read32(0x1000D440) && !spr.write32(0x1000E000,0));
    check(refused([&]{spr.write32(0x1000D000,0x100);}));
    std::printf("IPU setup register and command tests passed\n");
}
