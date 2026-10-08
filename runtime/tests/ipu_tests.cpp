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
    check(refused([&]{ipu.write32(0x10002000,0x60000000);})); // SETVQ without its two quadwords.
    ipu.writeFifo(0x10007010,{0x00020001,0,0,0});ipu.writeFifo(0x10007010,{0,0,0,0x80000000});
    check(ipu.write32(0x10002000,0x60000000) && ipu.vq_loaded && ipu.vq_clut[0]==1 && ipu.vq_clut[1]==2 && ipu.vq_clut[15]==0x8000);
    check(ipu.write32(0x10002000,0x90000000) && ipu.thresholds==0);
    check(ipu.write32(0x10002010,0x40000000) && ipu.resets==2 && ipu.commands==5);
    // Decode parameters (MP1 here) are retained and read back; other control bits are refused.
    check(ipu.write32(0x10002010,0x00800000) && ipu.read32(0x10002010)==0x00800000u);
    check(refused([&]{ipu.write32(0x10002010,0x00000100);}));
    // Decode commands and IPU DMA starts are refused; stopped channel registers are retained.
    check(refused([&]{ipu.write32(0x10002000,0x70000000);}));
    check(refused([&]{ipu.write32(0x1000B400,0x100);}));
    check(ipu.write32(0x1000B020,0x12345) && ipu.read32(0x1000B020)==0x2345u && !ipu.read32(0x10003000));
    // Stopped scratchpad channels: registers retained (QWC 16 bits, SADR within 16 KB), STR refused.
    StoppedDmaChannels spr{0x1000D000,0x1000D400};
    check(spr.write32(0x1000D400,0) && spr.write32(0x1000D420,0x12345) && spr.read32(0x1000D420)==0x2345u);
    check(spr.write32(0x1000D480,0x7FFF) && spr.read32(0x1000D480)==0x3FF0u && !spr.read32(0x1000D440) && !spr.write32(0x1000E000,0));
    check(refused([&]{spr.write32(0x1000D000,0x100);}));
    std::printf("IPU setup register and command tests passed\n");
}
