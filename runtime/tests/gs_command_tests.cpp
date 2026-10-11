#include "gs/gif_dma_staging.h"
#include "gs/texture_gif_startup.h"
#include "gs/cpu/cpu_display.h"
#include <iostream>
#include <set>
#include <source_location>
#include <cstdlib>
void check(bool value,std::source_location at=std::source_location::current()) {if(!value){std::cerr<<"GS assertion failed at line "<<at.line()<<'\n';std::exit(1);}}
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
        auto bad=t;const std::uint64_t tri=0x3000000000008002ULL|(1ULL<<46)|(1ULL<<47);std::memcpy(bad.data()+8*16,&tri,8); // PRE line (unsupported).
        const auto regs=gs.state.regs;gs.seedForTest(inside,0xDEADBEEF);
        bool rejected=false;try{gs.transfer(bad);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && gs.transfers==1 && gs.state.regs==regs && gs.word(inside).value==0xDEADBEEF);
        bool odd=false;try{gs.transfer(std::span<const std::uint8_t>(t.data(),8));}catch(const std::runtime_error&){odd=true;}
        check(odd);
    }
    {
        // Triangles. An 8x8 square drawn as a two-triangle strip, then as a two-triangle fan, covers each
        // pixel exactly once (top-left fill rule on the shared diagonal); Gouraud interpolates color and a
        // flat triangle takes its last vertex's color.
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        auto ad=[&](std::vector<std::pair<unsigned,std::uint64_t>> w){q(0x1000000000000000ULL|w.size(),0xE);for(auto [r,v]:w)q(v,r);};
        auto xyz=[](unsigned x,unsigned y){return std::uint64_t(x*16)|(std::uint64_t(y*16)<<16);};
        ad({{0x1A,1},{0x4C,0x10000},{0x4E,0x101000020ULL},{0x18,0},{0x40,0x003F0000003F0000ULL},{0x47,0x30000},{0x01,0x80402010}});
        ad({{0x00,4},{0x05,xyz(0,0)},{0x05,xyz(8,0)},{0x05,xyz(0,8)},{0x05,xyz(8,8)}}); // TRISTRIP.
        GsCommandSink gs;gs.transfer(t);
        check(gs.frame_triangles==2 && gs.frame_pixels==64);
        check(gs.word(GsCommandSink::ct32Word(0,1,0,0)).value==0x80402010 && gs.word(GsCommandSink::ct32Word(0,1,7,7)).known==UINT32_MAX);
        check(!gs.word(GsCommandSink::ct32Word(0,1,8,0)).known && !gs.word(GsCommandSink::ct32Word(0,1,0,8)).known); // Right/bottom edges excluded.
        t.clear();
        ad({{0x00,5},{0x05,xyz(16,0)},{0x05,xyz(24,0)},{0x05,xyz(24,8)},{0x05,xyz(16,8)}}); // TRIFAN around (16,0).
        gs.transfer(t);check(gs.frame_triangles==4 && gs.frame_pixels==128);
        t.clear();
        // Gouraud (PRIM TRIANGLE | IIP): red 0 -> 160 along the top edge; pixel (5,0) is halfway.
        ad({{0x00,3|8},{0x01,0x80000000},{0x05,xyz(0,16)},{0x01,0x800000A0},{0x05,xyz(10,16)},{0x01,0x80000000},{0x05,xyz(0,26)}});
        gs.transfer(t);check(gs.frame_triangles==5 && (gs.word(GsCommandSink::ct32Word(0,1,5,16)).value&0xFF)==80);
        t.clear();
        // Flat: the last (kicking) vertex's color fills the triangle.
        ad({{0x00,3},{0x01,0x11},{0x05,xyz(32,16)},{0x01,0x22},{0x05,xyz(42,16)},{0x01,0x80000033},{0x05,xyz(32,26)}});
        gs.transfer(t);check(gs.word(GsCommandSink::ct32Word(0,1,33,17)).value==0x80000033);
        // XYZ3 queues a vertex without drawing.
        t.clear();ad({{0x00,3},{0x05,xyz(48,0)},{0x05,xyz(56,0)},{0x0D,xyz(48,8)}});
        const auto pixels=gs.frame_pixels;gs.transfer(t);check(gs.frame_pixels==pixels && gs.frame_triangles==6);
    }
    {
        // PSMT4 layout: explicit entries from the public column/block layout, then a bijection check
        // over one 128x128 page (16,384 nibbles in its 2,048 words).
        auto n=[](unsigned x,unsigned y){return GsCommandSink::t4Nibble(0,2,x,y);};
        // Entries of the public PSMT4 column layout (column 0 rows 0-3, column 1 row 4).
        check(n(0,0)==0 && n(1,0)==8 && n(2,0)==32 && n(3,0)==40 && n(4,0)==64 && n(7,0)==104 && n(8,0)==2 && n(31,0)==110);
        check(n(0,1)==16 && n(0,2)==65 && n(4,2)==1 && n(0,3)==81 && n(4,3)==17 && n(31,3)==63);
        check(n(0,4)==192 && n(4,4)==128 && n(0,6)==128+1 && n(4,6)==128+65); // Odd column: 64 stride flipped.
        check(n(32,0)==2*512 && n(0,16)==1*512 && n(64,0)==8*512 && n(0,32)==4*512 && n(0,64)==16*512); // Block order.
        std::set<std::size_t> seen;for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x)seen.insert(n(x,y));
        check(seen.size()==16384 && *seen.rbegin()==16383);
        check(GsCommandSink::t4Nibble(0,4,128,0)==2048*8); // Second page in the row (TBW 4 = 256 pixels).
    }
    {
        // PSMT8 layout: entries of the public column/block layout, then a bijection over one 128x64 page.
        auto b=[](unsigned x,unsigned y){return GsCommandSink::t8Byte(0,2,x,y);};
        check(b(0,0)==0 && b(1,0)==4 && b(2,0)==16 && b(4,0)==32 && b(8,0)==2 && b(0,1)==8);
        check(b(0,2)==33 && b(4,2)==1 && b(0,3)==41 && b(0,4)==96 && b(4,4)==64 && b(0,6)==65); // Rows 2-3, odd column.
        check(b(16,0)==256 && b(0,16)==512 && b(32,0)==1024 && b(0,32)==2048 && b(64,0)==4096); // Block order.
        std::set<std::size_t> seen;for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)seen.insert(b(x,y));
        check(seen.size()==8192 && *seen.rbegin()==8191);
        check(GsCommandSink::t8Byte(0,4,128,0)==2048*4); // Second page in the row (TBW 4 = 256 pixels).
    }
    {
        // PSMT8 textured sprite with a 256-entry CT32 CLUT (16x16, index bits 3 and 4 swapped).
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        auto ad=[&](std::vector<std::pair<unsigned,std::uint64_t>> w,bool eop=false){q(0x1000000000000000ULL|(eop?0x8000:0)|w.size(),0xE);for(auto [r,v]:w)q(v,r);};
        auto color=[](unsigned i){return 0x80000000u|i;};
        ad({{0x1A,1},{0x01,0x80808080},{0x4C,0x10000},{0x4E,0x101000020ULL},{0x18,0},{0x40,0x0007000000070000ULL},{0x47,0x30000},{0x46,1},{0x45,0},{0x14,0},{0x08,0},
            {0x50,(0x100ULL|(1ULL<<16))<<32},{0x51,0},{0x52,0x0000001000000010ULL},{0x53,0}});
        q(0x0800000000000000ULL|64,0);
        for(unsigned s=0;s<256;s+=4) {
            auto entry=[&](unsigned k){const unsigned p=s+k;return color((p&~0x18u)|((p&0x08u)<<1)|((p&0x10u)>>1));};
            q((std::uint64_t(entry(1))<<32)|entry(0),(std::uint64_t(entry(3))<<32)|entry(2));
        }
        // Texture: one PSMT8 page (one CT32 page of words) at block 0x200 whose bytes are 0x10..0x40.
        ad({{0x50,(std::uint64_t(0x200)|(2ULL<<16))<<32},{0x52,0x0000002000000040ULL},{0x53,0}});
        q(0x0800000000000000ULL|512,0);for(unsigned k=0;k<512;++k)q(0x4030201040302010ULL,0x4030201040302010ULL);
        const std::uint64_t tex0=0x200|(2ULL<<14)|(0x13ULL<<20)|(7ULL<<26)|(6ULL<<30)|(1ULL<<34)|(0x100ULL<<37)|(1ULL<<61);
        ad({{0x06,tex0},{0x00,0x156&~0x40},{0x03,0},{0x05,0},{0x03,(8ULL<<4)<<16|(8ULL<<4)},{0x05,0x00800080}},true);
        GsCommandSink gs;gs.transfer(t);
        check(gs.clut_loads==1 && gs.image_pixels==256+2048 && gs.frame_sprites==1 && gs.frame_pixels==64);
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
            check(gs.word(GsCommandSink::ct32Word(0,1,x,y)).value==color((unsigned(GsCommandSink::t8Byte(0x200,2,x,y)%4)+1)*16));
    }
    {
        // Textured sprite: a 16-entry CT32 CLUT and a PSMT4 texture uploaded as CT32 words, drawn
        // 1:1 with nearest filtering, then blended; and a refused group leaves nothing behind.
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        auto ad=[&](std::vector<std::pair<unsigned,std::uint64_t>> w,bool eop=false){q(0x1000000000000000ULL|(eop?0x8000:0)|w.size(),0xE);for(auto [r,v]:w)q(v,r);};
        ad({{0x1A,1},{0x01,0x80808080},{0x4C,0x10000},{0x4E,0x101000020ULL},{0x18,0},{0x40,0x0007000000070000ULL},{0x47,0x30000},{0x46,1},{0x45,0},{0x14,0},{0x08,0},
            {0x50,0x00000100ULL<<32},{0x51,0},{0x52,0x0000000200000008ULL},{0x53,0}},true); // EOP: later packets in the transfer still run.
        q(0x0800000000000004ULL,0); // IMAGE, 4 qwords: CLUT entry i = 0x80 alpha, red i*16.
        for(unsigned i=0;i<16;i+=2)if(i%4==0)q((std::uint64_t(0x80000000u|((i+1)*16))<<32)|(0x80000000u|(i*16)),(std::uint64_t(0x80000000u|((i+3)*16))<<32)|(0x80000000u|((i+2)*16)));
        // Texture: one CT32 page (64x32 words) of 0x76543210 at block 0x200 (TBW 2: a 128-pixel T4 page).
        ad({{0x50,(std::uint64_t(0x200)|(2ULL<<16))<<32},{0x52,0x0000002000000040ULL},{0x53,0}});
        q(0x0800000000000000ULL|512,0);for(unsigned k=0;k<512;++k)q(0x7654321076543210ULL,0x7654321076543210ULL);
        const std::uint64_t tex0=0x200|(2ULL<<14)|(0x14ULL<<20)|(7ULL<<26)|(7ULL<<30)|(1ULL<<34)|(0x100ULL<<37)|(1ULL<<61);
        ad({{0x06,tex0},{0x00,0x156&~0x40},{0x03,0},{0x05,0},{0x03,(8ULL<<4)<<16|(8ULL<<4)},{0x05,0x00800080}},true);
        GsCommandSink gs;gs.transfer(t);
        check(gs.clut_loads==1 && gs.image_pixels==16+2048 && gs.frame_sprites==1 && gs.frame_pixels==64 && !gs.state.trx);
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
            const auto index=unsigned(GsCommandSink::t4Nibble(0x200,2,x,y)%8);
            check(gs.word(GsCommandSink::ct32Word(0,1,x,y)).value==(0x80000000u|(index*16)));
        }
        // A group whose second transfer is refused (STQ sprite) leaves no trace of its first, blended
        // sprite (ALPHA (Cs-0)*FIX>>7+0, FIX 0x40); alone, that sprite halves the texel colors.
        const auto before=gs.word(GsCommandSink::ct32Word(0,1,1,1));const auto writes=gs.register_writes;
        std::vector<std::uint8_t> good;std::swap(t,good);
        ad({{0x42,0x00000040000000A8ULL},{0x00,0x156},{0x03,0},{0x05,0},{0x03,(8ULL<<4)<<16|(8ULL<<4)},{0x05,0x00800080}});
        auto blended=t;t.clear();ad({{0x00,0x56},{0x05,0},{0x05,0x00800080}},true);auto refused=t;
        const std::array<std::span<const std::uint8_t>,2> group={std::span<const std::uint8_t>(blended),std::span<const std::uint8_t>(refused)};
        bool rejected=false;try{gs.transferAll(group);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==before.value && gs.register_writes==writes && gs.transfers==1);
        gs.transfer(blended);
        const auto index=unsigned(GsCommandSink::t4Nibble(0x200,2,1,1)%8);
        check(gs.frame_sprites==2 && gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==(0x80000000u|(index*8)));
    }
    {
        // Depth test: a sprite writes Z 100 (Z32 at page 32); GEQUAL passes with Z 100, GREATER fails with
        // Z 100 and passes with Z 101, NEVER draws nothing; GEQUAL over unwritten depth is refused.
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        auto draw=[&](unsigned ztst,std::uint32_t z,std::uint32_t color){
            t.clear();q(0x1000000000008007ULL,0xE); // NLOOP 7, EOP, A+D.
            q(0x10000,0x4C);q(0x20,0x4E);q(0,0x18);q(0x0003000000030000ULL,0x40); // FRAME, ZBUF Z32 writes, SCISSOR 0..3.
            q(0x10000|(std::uint64_t(ztst)<<17),0x47);q(color,0x01);q(6,0x00);       // TEST ZTE, RGBAQ, PRIM sprite.
            q(0x1000000000008002ULL,0xE);q(0x0000000000000000ULL|(std::uint64_t(z)<<32),0x05);q(0x0000000000200020ULL|(std::uint64_t(z)<<32),0x05);
            return t;};
        GsCommandSink gs;
        gs.transfer(draw(1,100,0x11));check(gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==0x11);
        gs.transfer(draw(2,100,0x22));check(gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==0x22);
        gs.transfer(draw(3,100,0x33));check(gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==0x22);
        gs.transfer(draw(3,101,0x44));check(gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==0x44);
        gs.transfer(draw(0,500,0x55));check(gs.word(GsCommandSink::ct32Word(0,1,1,1)).value==0x44);
        GsCommandSink fresh;bool refused=false;try{fresh.transfer(draw(2,1,0x66));}catch(const std::runtime_error&){refused=true;}
        check(refused);
    }
    {
        // Display decoding for presentation: DISPFB FBW 1 (64 pixels), DISPLAY 64x2; one written pixel
        // becomes BGRA, the rest are reported as unwritten; an unsupported display PSM is refused.
        GsCommandSink gs;gs.seedForTest(GsCommandSink::ct32Word(0,1,1,0),0x00332211);
        const auto f=decodeDisplay(gs,1ULL<<9,(63ULL<<32)|(1ULL<<44));
        check(f.width==64 && f.height==2 && f.bgra[1]==0x112233 && f.unwritten==127);
        const auto offset=decodeDisplay(gs,(1ULL<<9)|(1ULL<<32),(0ULL<<32)|(0ULL<<44)); // DBX 1: 1x1 at the written pixel.
        check(offset.width==1 && offset.height==1 && offset.bgra[0]==0x112233 && !offset.unwritten);
        bool refused=false;try{decodeDisplay(gs,(1ULL<<9)|(2ULL<<15),63ULL<<32);}catch(const std::runtime_error&){refused=true;}
        check(refused);
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
