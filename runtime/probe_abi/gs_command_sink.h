#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

// Scoped software command consumption, separate from DMA and presentation.
// Unwritten VRAM bits remain unknown; construction does not emulate a GS clear.
class GsCommandSink {
public:
    struct Word { std::uint32_t value=0, known=0; };
    GsCommandSink():vram_(1024*1024) {}
    void resetDrawingState() {
        // CSR reset establishes SCANMSK/FBA/PABE defaults. Do not clear VRAM.
        registers={};drawing_reset_=true;
    }
    static std::size_t address(unsigned page,unsigned width,unsigned x,unsigned y,bool depth) {
        // PSMCT32/PSMZ24: 64x32 pages, 8x8 blocks. Public GS bit layout,
        // independently expressed as bit placement rather than reference tables.
        unsigned bx=(x%64)/8, by=(y%32)/8;
        unsigned block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
        unsigned px=x%8, py=y%8;
        unsigned pixel=(px&1)|((py&1)<<1)|((px&6)<<1)|((py&6)<<3);
        std::size_t word=((std::size_t(page)+(y/32)*width+x/64)*2048+block*64+pixel);
        if(depth)word^=1536;
        return word%(1024*1024);
    }
    Word word(std::size_t index) const {return vram_.at(index);}
    void seedForTest(std::size_t index,std::uint32_t value) {vram_.at(index)={value,UINT32_MAX};}
    void consume(std::span<const std::uint8_t> packet) {
        auto read=[&](std::size_t offset) {std::uint64_t v;std::memcpy(&v,packet.data()+offset,8);return v;};
        auto reject=[](){throw std::runtime_error("Unsupported GS sprite packet/state");};
        if(!drawing_reset_ || packet.size()!=240)reject();
        if(read(0)!=0x100000000000800EULL || read(8)!=0xE)reject();
        constexpr std::array<unsigned,14> regs={0x4C,0x4E,0x18,0x40,0x1A,0x46,0x45,0x47,0x47,0,1,5,5,0x47};
        std::array<std::uint64_t,14> v{};
        for(unsigned i=0;i<14;++i) {if(read(24+i*16)!=regs[i])reject();v[i]=read(16+i*16);}
        unsigned width=unsigned((v[0]>>16)&63),fbp=unsigned(v[0]&511),zbp=unsigned(v[1]&511);
        if(!width || ((v[0]>>24)&63)!=0 || (v[0]&0xC0C0FE00ULL)
            || ((v[1]>>24)&63)!=1 || (v[1]&~0x1000001FFULL)!=0x1000000ULL
            || v[4]%2!=1 || v[5]%2!=1 || v[6]%2!=0 || v[7]!=0x50000
            || v[8]!=0x30000 || v[9]!=6 || v[13]!=0x50000)reject();
        // At the draw: alpha/destination tests disabled, depth ALWAYS, no blend,
        // texture, fog or dither. ZMSK is supported; FRAME mask preserves bits.
        unsigned ox=unsigned(v[2]&65535),oy=unsigned((v[2]>>32)&65535);
        if(v[2]&0xFFFF0000FFFF0000ULL || v[3]&0xF800F800F800F800ULL)reject();
        auto coordinate=[&](std::uint64_t vertex,bool y) {
            int n=int((vertex>>(y?16:0))&65535)-int(y?oy:ox);
            if(n%16)reject();return n/16;
        };
        int x0=coordinate(v[11],false),y0=coordinate(v[11],true);
        int x1=coordinate(v[12],false),y1=coordinate(v[12],true);
        if(x1<x0 || y1<y0)reject();
        x0=std::max(x0,int(v[3]&2047));y0=std::max(y0,int((v[3]>>32)&2047));
        x1=std::min(x1,int((v[3]>>16)&2047)+1);y1=std::min(y1,int((v[3]>>48)&2047)+1);
        if(x0<0 || y0<0 || x1>int(width*64) || y1>2048)reject();
        std::uint32_t color=std::uint32_t(v[10]),mask=std::uint32_t(v[0]>>32);
        std::uint32_t z=std::uint32_t(v[12]>>32)&0xFFFFFF;
        // All rejection occurs above: no partial effects on malformed requests.
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
            auto& c=vram_[address(fbp,width,unsigned(x),unsigned(y),false)];
            c.value=(c.value&mask)|(color&~mask);c.known|=~mask;
            if(!(v[1]&(1ULL<<32))) {
                auto& d=vram_[address(zbp,width,unsigned(x),unsigned(y),true)];
                d.value=(d.value&0xFF000000)|z;d.known|=0xFFFFFF;
            }
            ++pixels;
        }
        registers=v;++packets;++sprites;
    }
    unsigned packets=0,sprites=0;
    std::uint64_t pixels=0;
    std::array<std::uint64_t,14> registers{};
private:
    bool drawing_reset_=false;
    std::vector<Word> vram_;
};
