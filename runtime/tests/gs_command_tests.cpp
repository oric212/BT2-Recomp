#include "gs/cpu/gs_command_sink.h"
#include "gs/textures/gs_texture_names.h"
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
        check(gs.transfers==1 && gs.state.regs[0x4C]==0x10000 && gs.state.regs[0x40]==0x0007000000070000ULL);
        // A later refused register write rejects the whole transfer: no register effect.
        auto bad=t;const std::uint64_t undefined=0x12;std::memcpy(bad.data()+6*16+8,&undefined,8); // TEST_1's A+D slot: register 0x12.
        auto changed=bad;const std::uint64_t frame=0x20000;std::memcpy(changed.data()+16,&frame,8); // A FRAME_1 the refusal must undo.
        const auto regs=gs.state.regs;
        bool rejected=false;try{gs.transfer(changed);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && gs.transfers==1 && gs.state.regs==regs);
        bool odd=false;try{gs.transfer(std::span<const std::uint8_t>(t.data(),8));}catch(const std::runtime_error&){odd=true;}
        check(odd);
        // The GPU renderer draws: a line kick, a perspective-textured, fogged triangle, a PSMT8
        // host-to-local transfer with its IMAGE data and a CT16 CLUT load are all accepted. Packet framing,
        // undefined registers and an empty local-to-host transfer are refused.
        auto line=t;const std::uint64_t tri=0x3000000000008002ULL|(1ULL<<46)|(1ULL<<47);std::memcpy(line.data()+8*16,&tri,8); // PRE line.
        GsCommandSink gpu;gpu.transfer(line);
        check(gpu.transfers==1);
        std::vector<std::uint8_t> p;
        auto g=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);p.insert(p.end(),b,b+16);};
        g(0x1000000000000005ULL,0xE);
        g(0x33,0x00);g(0,0x04);g(0,0x04);g(0,0x04); // PRIM triangle, TME, FGE (STQ): three XYZF2 kicks.
        g(0x13ULL<<56,0x50);                         // BITBLTBUF: PSMT8 destination.
        g(0x1000000000000003ULL,0xE);g(0x0000000400000010ULL,0x52);g(0,0x53);g((1ULL<<61)|(2ULL<<51)|(0x13ULL<<20),0x06); // TRXREG 16x4, TRXDIR 0; TEX0 CLD 1, CT16 CLUT.
        g(0x0800000000008004ULL,0);g(0,0);g(0,0);g(0,0);g(0,0); // IMAGE, 4 quadwords, EOP.
        gpu.transfer(p);
        check(gpu.transfers==2);
        auto refusedByGpu=[&](std::vector<std::uint8_t> packet) {try{gpu.transfer(packet);}catch(const std::runtime_error&){return true;}return false;};
        p.clear();g(0x1000000000008001ULL,0xE);g(1,0x53);check(refusedByGpu(p)); // TRXDIR 1: local to host.
        p.clear();g(0x1000000000008001ULL,0xE);g(0,0x12);check(refusedByGpu(p)); // Undefined register.
        p.clear();g(0x1000000000008002ULL,0xE);g(0,0x00);check(refusedByGpu(p)); // PACKED data exceeds the transfer.
        check(gpu.transfers==2);
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
    // The startup's clear (PACKED A+D: frame and depth buffers, offset, scissor, test, a sprite) and its
    // texture configuration (PABE, registers 8 and 9) are ordinary transfers: the register file follows
    // every write.
    std::vector<std::uint8_t> p(240);
    auto put=[&](unsigned offset,std::uint64_t value){std::memcpy(p.data()+offset,&value,8);};
    put(0,0x100000000000800EULL);put(8,14);
    std::array<unsigned,14> regs={0x4C,0x4E,0x18,0x40,0x1A,0x46,0x45,0x47,0x47,0,1,5,5,0x47};
    std::array<std::uint64_t,14> values={0x10000,0x1000020,0,0x0007000000070000ULL,1,1,0,0x50000,0x30000,6,0x3F80000012345678ULL,0,0x0000000900800080ULL,0x50000};
    for(unsigned i=0;i<14;++i){put(16+i*16,values[i]);put(24+i*16,regs[i]);}
    GsCommandSink gs;gs.transfer(p);
    check(gs.register_writes==14 && gs.state.regs[0x4C]==0x10000 && gs.state.regs[0]==6 && gs.state.regs[1]==0x3F80000012345678ULL && gs.state.regs[0x47]==0x50000);
    std::vector<std::uint8_t> config(64);
    auto configPut=[&](unsigned offset,std::uint64_t value){std::memcpy(config.data()+offset,&value,8);};
    configPut(0,0x1000000000008003ULL);configPut(8,14);
    configPut(16,1);configPut(24,0x49);configPut(32,0x1234);configPut(40,8);configPut(48,0x5678);configPut(56,9);
    gs.transfer(config);
    check(gs.state.regs[0x49]==1 && gs.state.regs[8]==0x1234 && gs.state.regs[9]==0x5678 && gs.register_writes==17);
    // Independent address landmarks and complete-page bijection.
    check(GsCommandSink::address(0,1,8,0,false)==64);
    check(GsCommandSink::address(0,1,0,8,false)==128);
    check(GsCommandSink::address(0,1,32,0,false)==1024);
    check(GsCommandSink::address(0,1,0,0,true)==1536);
    std::set<std::size_t> addresses;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)addresses.insert(GsCommandSink::address(0,1,x,y,false));
    check(addresses.size()==2048 && *addresses.rbegin()==2047);
    {
        // IMAGE data continuing in later transfers: a 4x2 CT32 upload whose tag carries 2 quadwords,
        // one in the tag's transfer and one in the next; then a new tag works again.
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        q(0x1000000000000004ULL,0xE);q(1ULL<<48,0x50);q(0,0x51);q(0x0000000200000004ULL,0x52);q(0,0x53);
        q(0x0800000000000002ULL,0);q(0x0000000200000001ULL,0x0000000400000003ULL);
        GsCommandSink gs;gs.transfer(t);
        check(gs.state.image_qwords_left==1);
        t.clear();q(0x0000000600000005ULL,0x0000000800000007ULL);q(0x1000000000008001ULL,0xE);q(0,0x51);
        gs.transfer(t);
        check(gs.state.image_qwords_left==0 && gs.register_writes==5); // The next tag's A+D write is read as one.
    }
    {
        // Local->host transfer (TRXDIR 1): a 4x2 CT32 rectangle is followed through the FIFO (the GPU renderer
        // supplies its pixels; here they read as zero); FINISH writes are counted.
        GsCommandSink gs;
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        q(0x1000000000008005ULL,0xE);q(0x40ULL|(1ULL<<16),0x50);q(2ULL|(1ULL<<16),0x51);q(0x0000000200000004ULL,0x52);q(0,0x61);q(1,0x53);
        check(!gs.downloadQwords());
        gs.transfer(t);
        check(gs.state.download && gs.state.finishes==1 && gs.downloadQwords()==2);
        std::array<std::uint8_t,16> row{};row.fill(0xAA);gs.readDownload(row);
        check(row[0]==0 && row[15]==0 && gs.downloadQwords()==1);
        std::array<std::uint8_t,32> rest{};gs.readDownload(rest);check(!gs.downloadQwords());
        // CT24 is three bytes per pixel: 4 x 2 pixels are 24 bytes, two quadwords.
        t.clear();q(0x1000000000008002ULL,0xE);q(0x40ULL|(1ULL<<16)|(1ULL<<24),0x50);q(1,0x53);gs.transfer(t);
        check(gs.state.finishes==1 && gs.state.dl_psm==1 && gs.downloadQwords()==2);
        // Other formats are the renderer's to read: an 8-bit 16 x 32 rectangle is 32 quadwords.
        t.clear();q(0x1000000000008004ULL,0xE);q(0x40ULL|(1ULL<<16)|(0x13ULL<<24),0x50);q(0,0x51);q(0x0000002000000010ULL,0x52);q(1,0x53);gs.transfer(t);
        check(gs.downloadQwords()==32);gs.readDownload(row);check(gs.downloadQwords()==31);
        GsCommandSink idle;bool refused=false;try{idle.readDownload(row);}catch(const std::runtime_error&){refused=true;}check(refused);
    }
    {
        // Texture names in step with the renderer: with names attached, a transfer goes on in pieces that end
        // before an upload's data and before a CLUT load, and the pieces put together are the transfer.
        std::vector<std::uint8_t> t;
        auto q=[&](std::uint64_t lo,std::uint64_t hi){std::uint8_t b[16];std::memcpy(b,&lo,8);std::memcpy(b+8,&hi,8);t.insert(t.end(),b,b+16);};
        q(0x1000000000000004ULL,0xE);                 // NLOOP 4, A+D.
        q(0x0000000000000000ULL|(0x300ULL<<32)|(1ULL<<48),0x50); // BITBLTBUF: DBP 0x300, DBW 1, PSMCT32.
        q(0,0x51);q(8ULL|(2ULL<<32),0x52);q(0,0x53);    // TRXPOS 0, TRXREG 8 x 2, TRXDIR host to local.
        const std::size_t image_data=t.size()+16;
        q(0x0800000000008004ULL,0);                   // NLOOP 4, EOP, IMAGE.
        for(unsigned i=0;i<4;++i)q(0x0403020108070605ULL+i,0x0C0B0A090F0E0D0CULL+i);
        q(0x1000000000000002ULL,0xE);                 // NLOOP 2, A+D.
        const std::size_t clut_load=t.size();
        q(0x14ULL<<20|6ULL<<26|6ULL<<30|0x300ULL<<37|1ULL<<61,0x06); // TEX0_1: PSMT4 64 x 64, CBP 0x300, CLD 1.
        q(0x16,0x00);                                 // PRIM: triangle strip with TME.
        GsTextureNames names;GsCommandSink gs;gs.texture_names=&names;
        std::vector<std::vector<std::uint8_t>> pieces;
        gs.on_accepted=[&](std::span<const std::uint8_t> d){pieces.emplace_back(d.begin(),d.end());};
        gs.transfer(t);
        check(pieces.size()==3 && pieces[0].size()==image_data && pieces[0].size()+pieces[1].size()==clut_load);
        std::vector<std::uint8_t> joined;for(const auto& p:pieces)joined.insert(joined.end(),p.begin(),p.end());
        check(joined==t);
        // The CLUT load read the uploaded colours: entry 0 is the first pixel.
        check(names.clut()[0]==0x0605 && names.clut()[256]==0x0807);
        // Without names the transfer goes on whole.
        GsCommandSink whole;pieces.clear();
        whole.on_accepted=[&](std::span<const std::uint8_t> d){pieces.emplace_back(d.begin(),d.end());};
        whole.transfer(t);check(pieces.size()==1 && pieces[0]==t);
        // The GS thread (gs/gpu/gpu_gs_mirror.h): the game's sink without names hands transfers to
        // on_accepted_transfer; a sink with names, given the accepted
        // transfers one by one, hands the renderer the same bytes in step and names as the direct path did,
        // also when IMAGE data continues into the next transfer of a group.
        const std::size_t split=image_data+32; // Two of the four IMAGE quadwords in the first transfer.
        const std::vector<std::uint8_t> first(t.begin(),t.begin()+std::ptrdiff_t(split)),second(t.begin()+std::ptrdiff_t(split),t.end());
        GsCommandSink game;
        std::vector<std::vector<std::uint8_t>> transfers,packets;
        game.on_accepted=[&](std::span<const std::uint8_t> d){packets.emplace_back(d.begin(),d.end());};
        game.on_accepted_transfer=[&](std::span<const std::uint8_t> d){transfers.emplace_back(d.begin(),d.end());};
        const std::array<std::span<const std::uint8_t>,2> group{first,second};
        game.transferAll(group);
        check(transfers.size()==2 && transfers[0]==first && transfers[1]==second && packets.empty());
        GsTextureNames thread_names;GsCommandSink named;named.texture_names=&thread_names;pieces.clear();
        named.on_accepted=[&](std::span<const std::uint8_t> d){pieces.emplace_back(d.begin(),d.end());};
        for(const auto& data:transfers)named.transfer(data);
        joined.clear();for(const auto& p:pieces)joined.insert(joined.end(),p.begin(),p.end());
        check(joined==t && thread_names.clut()==names.clut() && thread_names.memory()==names.memory());
    }
    std::cout<<"GS scoped command tests passed\n";
}
