#include "gif_dma_staging.h"
#include "texture_gif_startup.h"
#include <iostream>
#include <set>
void check(bool value) {if(!value)throw std::runtime_error("GS assertion failed");}
int main() {
    {
        // General GIF transfer: PACKED A+D environment, PACKED RGBAQ/XYZ2 sprite kick (synthetic values).
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        q(0x1000000000000007ULL,0xE); // NLOOP 7, A+D, one register (EOP in the second tag).
        q(0x0000000000010000ULL,0x4C);   // FRAME_1: FBP 0, FBW 1 (64 pixels), PSMCT32, no mask.
        q(0x0000000101000020ULL,0x4E);   // ZBUF_1: ZBP 32, Z24, ZMSK.
        q(0x0000000000000000ULL,0x18);   // XYOFFSET_1 0.
        q(0x0007000000070000ULL,0x40);   // SCISSOR_1 0..7.
        q(1,0x1A);q(0x30000,0x47);        // PRMODECONT AC=1; TEST ZTE ALWAYS.
        q(6,0x00);                        // PRIM sprite.
        q(0x3000000000008002ULL|(1ULL<<46)|(6ULL<<47),0x551); // NLOOP 2, EOP, PRE sprite; PACKED RGBAQ/XYZ2/XYZ2 (NREG 3).
        q(0x0000000200000001ULL,0x0000000400000003ULL); // RGBAQ R1 G2 B3 A4.
        q(0x0000001000000010ULL,0);       // XYZ2 (1,1).
        q(0x0000004000000040ULL,0);       // XYZ2 (4,4): kick covers pixels 1..3.
        q(0,0);q(0,0);q(0,0);             // Second loop: RGBAQ 0, two vertices at (0,0): empty sprite.
        GsCommandSink gs;gs.transfer(t);
        check(gs.transfers==1 && gs.frame_sprites==2 && gs.frame_pixels==9 && gs.state.regs[0x4C]==0x10000);
        const auto inside=GsCommandSink::address(0,1,2,2,false),outside=GsCommandSink::address(0,1,4,4,false);
        check(gs.word(inside).value==0x04030201 && gs.word(inside).known==UINT32_MAX && gs.word(outside).known==0);
        // A later unsupported kick rejects the whole transfer: no register or VRAM effect.
        auto bad=t;const std::uint64_t tri=0x3000000000008002ULL|(1ULL<<46)|(3ULL<<47);std::memcpy(bad.data()+8*16,&tri,8); // PRE triangle.
        const auto regs=gs.state.regs;gs.seedForTest(inside,0xDEADBEEF);
        bool rejected=false;try{gs.transfer(bad);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && gs.transfers==1 && gs.state.regs==regs && gs.word(inside).value==0xDEADBEEF);
        bool odd=false;try{gs.transfer(std::span<const std::uint8_t>(t.data(),8));}catch(const std::runtime_error&){odd=true;}
        check(odd);
    }
    std::vector<std::uint8_t> p(240);
    auto put=[&](unsigned offset,std::uint64_t value){std::memcpy(p.data()+offset,&value,8);};
    put(0,0x100000000000800EULL);put(8,14);
    std::array<unsigned,14> regs={0x4C,0x4E,0x18,0x40,0x1A,0x46,0x45,0x47,0x47,0,1,5,5,0x47};
    std::array<std::uint64_t,14> values={0x10000,0x1000020,0,0x0007000000070000ULL,1,1,0,0x50000,0x30000,6,0x3F80000012345678ULL,0,0x0000000900800080ULL,0x50000};
    for(unsigned i=0;i<14;++i){put(16+i*16,values[i]);put(24+i*16,regs[i]);}
    const auto original=p;
    GsCommandSink gs;
    bool uninitialized_rejected=false;
    try{gs.consume(p);}catch(const std::runtime_error&){uninitialized_rejected=true;}
    check(uninitialized_rejected && gs.packets==0);
    gs.seedForTest(1000000,0xCAFEBABE);gs.resetDrawingState();
    check(gs.word(1000000).value==0xCAFEBABE && gs.word(1000000).known==UINT32_MAX);
    auto ca=GsCommandSink::address(0,1,7,7,false),za=GsCommandSink::address(32,1,7,7,true);
    gs.seedForTest(za,0xABFFFFFF);gs.consume(p);
    check(gs.pixels==64 && gs.packets==1 && gs.word(ca).value==0x12345678 && gs.word(ca).known==UINT32_MAX);
    check(gs.word(za).value==0xAB000009 && gs.word(za).known==UINT32_MAX);
    check(gs.word(GsCommandSink::address(32,1,0,0,true)).known==0xFFFFFF);
    check(gs.word(GsCommandSink::address(0,1,8,0,false)).known==0);
    // Independent address landmarks and complete-page bijection.
    check(GsCommandSink::address(0,1,8,0,false)==64);
    check(GsCommandSink::address(0,1,0,8,false)==128);
    check(GsCommandSink::address(0,1,32,0,false)==1024);
    check(GsCommandSink::address(0,1,0,0,true)==1536);
    std::set<std::size_t> addresses;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)addresses.insert(GsCommandSink::address(0,1,x,y,false));
    check(addresses.size()==2048 && *addresses.rbegin()==2047);
    // A late malformed register must leave earlier state and pixels untouched.
    put(24+13*16,0x7F);bool rejected=false;
    try{gs.consume(p);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && gs.packets==1 && gs.pixels==64);put(24+13*16,regs[13]);
    // Frame mask, disabled depth writes, clipping and lower/right exclusive edges.
    put(16,0x00FF00FF00010000ULL);put(32,0x101000020ULL);
    put(16+3*16,0x0006000200060002ULL);
    auto masked=GsCommandSink::address(0,1,2,2,false);gs.seedForTest(masked,0xAABBCCDD);
    put(16+12*16,0x0000000C00800080ULL);
    gs.consume(p);check(gs.pixels==89);
    check(gs.word(masked).value==0x12BB56DD);
    check(gs.word(za).value==0xAB000009);
    check(gs.word(GsCommandSink::address(32,1,2,2,true)).value==9);
    put(16+9*16,0x46);rejected=false;
    try{gs.consume(p);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && gs.pixels==89);
    // A transfer consumes commands before completion. Later reads require
    // explicit evidence and ordering; source mutation cannot draw partial data.
    GifDmaStaging dma;dma.gs.resetDrawingState();dma.supply(original);check(dma.readIdle()==0);dma.setCount(15);
    std::vector<std::uint8_t> ram(4096);std::copy(original.begin(),original.end(),ram.begin()+256);
    dma.setAddress(256,256,ram);ram[256+16]^=1;rejected=false;
    try{dma.start(0x101,ram);}catch(const GifPacketMismatch&){rejected=true;}
    check(rejected && dma.gs.packets==0 && dma.starts==0);ram[256+16]^=1;
    dma.start(0x101,ram);check(dma.qwc==0 && dma.madr==496 && dma.chcr==0x101 && dma.gs.packets==1);
    rejected=false;try{dma.readVifIdle();}catch(const std::runtime_error&){rejected=true;}check(rejected);
    std::array<std::uint32_t,33> pub={0,0,0x1FFEAF0,0,0x101,0,0x1FFEC40,0x40,0xE00,
        1,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0,
        0x1000A000,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0,
        0,0,0x1FFEAF0,0,0,0x1FFEB00};
    std::vector<std::uint8_t> evidence(372);std::memcpy(evidence.data(),pub.data(),sizeof(pub));
    std::copy(original.begin(),original.end(),evidence.begin()+132);dma.supplySync(evidence);
    check(dma.readVifIdle()==0 && dma.readStarted(false)==0x101 && dma.readStarted(true)==1);
    check(dma.completions==1 && dma.readVifStatus()==0x40 && dma.readVuIdle()==0 && dma.readGifStatus()==0);
    check(dma.readIdle()==1);dma.setCount(15);std::copy(original.begin(),original.end(),ram.begin()+512);
    dma.setAddress(512,512,ram);dma.start(0x101,ram);check(dma.starts==2 && dma.gs.packets==2);
    check(dma.readVifIdle()==0 && dma.readStarted(false)==0x101 && dma.readStarted(true)==1);
    check(dma.readVifStatus()==0x40 && dma.readVuIdle()==0 && dma.readGifStatus()==0 && dma.completions==2);
    rejected=false;try{dma.readIdle();}catch(const std::runtime_error&){rejected=true;}check(rejected);
    // A later identical request pair replays both packets from fresh state into a shared sink.
    auto repeat=dma.repeat();check(repeat.starts==0 && repeat.readIdle()==0);repeat.setCount(15);
    repeat.setAddress(256,256,ram);repeat.start(0x101,ram,dma.gs);
    check(repeat.readVifIdle()==0 && repeat.readStarted(false)==0x101 && repeat.readStarted(true)==1);
    check(repeat.readVifStatus()==0x40 && repeat.readVuIdle()==0 && repeat.readGifStatus()==0 && repeat.readIdle()==1);
    repeat.setCount(15);repeat.setAddress(512,512,ram);ram[512+16]^=1;rejected=false;
    try{repeat.start(0x101,ram,dma.gs);}catch(const GifPacketMismatch&){rejected=true;}
    check(rejected && repeat.starts==1 && dma.gs.packets==3);ram[512+16]^=1;
    repeat.start(0x101,ram,dma.gs);check(repeat.starts==2 && dma.gs.packets==4 && dma.starts==2 && repeat.gs.packets==0);
    rejected=false;try{(void)repeat.repeat();}catch(const std::runtime_error&){rejected=true;}check(rejected); // Pair not yet completed.
    const auto preserved=dma.gs.word(GsCommandSink::address(0,1,7,7,false));
    dma.gs.resetDrawingState();dma.gs.resetDrawingState();
    check(dma.gs.word(GsCommandSink::address(0,1,7,7,false)).value==preserved.value && dma.gs.packets==4 && dma.gs.pixels==256);
    std::vector<std::uint8_t> config(64);
    auto configPut=[&](unsigned offset,std::uint64_t value){std::memcpy(config.data()+offset,&value,8);};
    configPut(0,0x1000000000008003ULL);configPut(8,14);
    configPut(16,3);configPut(24,0x49);configPut(32,0x1234);configPut(40,8);configPut(48,0x5678);configPut(56,9);
    dma.gs.consumeConfiguration(config);
    check(dma.gs.pabe==1 && dma.gs.tex0[0]==0x1234 && dma.gs.tex0[1]==0x5678 && dma.gs.pixels==256);
    configPut(16,0);configPut(56,0x7F);rejected=false;
    try{dma.gs.consumeConfiguration(config);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && dma.gs.pabe==1 && dma.gs.configuration_packets==1);
    configPut(56,9);configPut(48,1ULL<<61);rejected=false;
    try{dma.gs.consumeConfiguration(config);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && dma.gs.tex0[1]==0x5678);
    configPut(48,0x5678);
    std::array<std::array<std::uint32_t,8>,5> config_publications{};
    for(unsigned i=0;i<4;++i)config_publications[i]={i==0||i==3?1u:i==2?0x101u:0u,0,0x1FFECE0,0,1,0x1FFED30,0,0};
    config_publications[4][2]=0x1FFED40;
    std::vector<std::uint8_t> config_evidence(224),config_ram(128);
    std::memcpy(config_evidence.data(),config_publications.data(),160);std::memcpy(config_evidence.data()+160,config.data(),64);std::memcpy(config_ram.data()+16,config.data(),64);
    TextureGifStartup transfer;transfer.supply(config_evidence);
    check(transfer.idle()==1 && transfer.pathIdle()==0);transfer.count(4);transfer.address(16,16,config_ram);
    config_ram[32]^=1;rejected=false;try{transfer.start(0x101,config_ram,dma.gs);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && transfer.starts==0 && transfer.phase==4);config_ram[32]^=1;
    transfer.start(0x101,config_ram,dma.gs);check(transfer.madr==80 && transfer.qwc==0 && transfer.poll()==0x101 && transfer.poll()==1 && transfer.pathComplete()==0);
    check(transfer.completions==1 && transfer.phase==8 && dma.gs.pixels==256);
    rejected=false;try{transfer.poll();}catch(const std::runtime_error&){rejected=true;}check(rejected);
    std::cout<<"GS scoped command tests passed\n";
}
