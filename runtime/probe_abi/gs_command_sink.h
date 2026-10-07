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
        registers={};tex0={};pabe=0;drawing_reset_=true;
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
        if(packet.size()!=240) {consumeConfiguration(packet);return;}
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
    void consumeConfiguration(std::span<const std::uint8_t> packet) {
        auto reject=[](){throw std::runtime_error("Unsupported GS configuration packet");};
        if(!drawing_reset_ || packet.size()<32 || packet.size()%16)reject();
        auto read=[&](std::size_t offset) {std::uint64_t value;std::memcpy(&value,packet.data()+offset,8);return value;};
        auto tag=read(0);const auto loops=tag&32767;
        // Packed, one A+D register, EOP, no PRE/primitive side effect.
        if(tag!=(0x1000000000008000ULL|loops) || read(8)!=14 || loops!=(packet.size()-16)/16)reject();
        auto next_tex0=tex0;auto next_pabe=pabe;
        for(std::size_t i=16;i<packet.size();i+=16) {
            const auto value=read(i),reg=read(i+8);
            if(reg==0x49)next_pabe=value&1;
            else if(reg==8 || reg==9) {
                // CLD requires a CLUT transaction, not a register-only update.
                if(value>>61)reject();
                next_tex0[reg-8]=value;
            } else reject();
        }
        tex0=next_tex0;pabe=next_pabe;++packets;++configuration_packets;
    }
    // General GIF transfer (frame path): GIFtags in PACKED, REGLIST or IMAGE form update the GS
    // register file; vertex kicks draw the supported primitives. The whole transfer is validated in
    // a dry run before any register or VRAM effect, so a rejected transfer changes nothing.
    void transfer(std::span<const std::uint8_t> data) {
        if(data.empty() || data.size()%16)throw std::runtime_error("Unsupported GIF transfer size");
        auto saved=state;
        try {run(data,false);} catch(...) {state=saved;throw;}
        state=saved;run(data,true);++transfers;
    }
    struct State {
        std::array<std::uint64_t,0x64> regs{};
        std::array<std::uint64_t,3> vertex_xyz{};std::array<std::uint64_t,3> vertex_rgbaq{};unsigned vertices=0;
        std::uint32_t q=0x3F800000; // ST packing sets Q; RGBAQ packing takes it.
    };
    State state;
    unsigned transfers=0,frame_sprites=0,register_writes=0;
    std::uint64_t frame_pixels=0;
private:
    void run(std::span<const std::uint8_t> data,bool apply) {
        auto read=[&](std::size_t offset) {std::uint64_t v;std::memcpy(&v,data.data()+offset,8);return v;};
        std::size_t at=0;
        while(at<data.size()) {
            const auto tag=read(at),regs=read(at+8);at+=16;
            const unsigned nloop=unsigned(tag&0x7FFF),flg=unsigned((tag>>58)&3);
            const unsigned nreg=unsigned((tag>>60)&15)?unsigned((tag>>60)&15):16;
            if((tag>>46)&1)write(0x00,(tag>>47)&0x7FF,apply); // PRE: PRIM from the tag.
            if(flg==0) {
                if(at+std::size_t(nloop)*nreg*16>data.size())throw std::runtime_error("GIF PACKED data exceeds transfer");
                for(unsigned l=0;l<nloop;++l)for(unsigned r=0;r<nreg;++r,at+=16)packed(unsigned((regs>>(4*r))&15),read(at),read(at+8),apply);
            } else if(flg==1) {
                const std::size_t words=std::size_t(nloop)*nreg;
                if(at+((words+1)/2)*16>data.size())throw std::runtime_error("GIF REGLIST data exceeds transfer");
                for(std::size_t w=0;w<words;++w) {
                    const unsigned d=unsigned((regs>>(4*(w%nreg)))&15);
                    if(d==0xE)throw std::runtime_error("GIF REGLIST A+D is undefined");
                    if(d!=0xF)write(d,read(at+w*8),apply);
                }
                at+=((words+1)/2)*16;
            } else throw std::runtime_error("Unsupported GIF IMAGE transfer");
            if(tag&0x8000)break; // EOP: the transfer's remaining data belongs to no tag.
        }
    }
    void packed(unsigned d,std::uint64_t lo,std::uint64_t hi,bool apply) {
        switch(d) {
        case 0x0: write(0x00,lo&0x7FF,apply);break;
        case 0x1: write(0x01,(lo&0xFF)|((lo>>24)&0xFF00)|((hi&0xFF)<<16)|(((hi>>32)&0xFF)<<24)|(std::uint64_t(state.q)<<32),apply);break;
        case 0x2: state.q=std::uint32_t(hi);write(0x02,lo,apply);break;
        case 0x3: write(0x03,(lo&0x3FFF)|(((lo>>32)&0x3FFF)<<16),apply);break;
        case 0x4: write((hi>>47)&1?0x0C:0x04,(lo&0xFFFF)|(((lo>>32)&0xFFFF)<<16)|(((hi>>4)&0xFFFFFF)<<32)|(((hi>>36)&0xFF)<<56),apply);break;
        case 0x5: write((hi>>47)&1?0x0D:0x05,(lo&0xFFFF)|(((lo>>32)&0xFFFF)<<16)|((hi&0xFFFFFFFF)<<32),apply);break;
        case 0xA: write(0x0A,((hi>>36)&0xFF)<<56,apply);break;
        case 0xE: write(unsigned(hi&0xFF),lo,apply);break;
        case 0xF: break;
        default: write(d,lo,apply);break; // TEX0_1/2, CLAMP_1/2, XYZF3, XYZ3: register-valued.
        }
    }
    void write(unsigned reg,std::uint64_t value,bool apply) {
        if(reg>=state.regs.size() || (reg>0x0F && reg<0x14) || reg==0x53 || reg==0x54)
            throw std::runtime_error("Unsupported GS register write");
        state.regs[reg]=value;if(apply)++register_writes;
        if(reg==0x00)state.vertices=0;
        if(reg!=0x04 && reg!=0x05 && reg!=0x0C && reg!=0x0D)return;
        const auto prim=(state.regs[0x1A]&1)?state.regs[0x00]:state.regs[0x1B];
        const unsigned type=unsigned(state.regs[0x00]&7),needed=type==6?2:type==3?3:0;
        if(!needed)throw std::runtime_error("Unsupported GS primitive type");
        if(state.vertices>=needed)state.vertices=0;
        state.vertex_xyz[state.vertices]=value;state.vertex_rgbaq[state.vertices]=state.regs[0x01];++state.vertices;
        if(state.vertices<needed || reg==0x0C || reg==0x0D)return;
        if(type!=6)throw std::runtime_error("Unsupported GS triangle drawing");
        sprite(prim,apply);state.vertices=0;
    }
    void sprite(std::uint64_t prim,bool apply) {
        auto reject=[](){throw std::runtime_error("Unsupported GS sprite state");};
        if(prim&0x7F8)reject(); // IIP/TME/FGE/ABE/AA1/FST: flat, untextured, unblended only.
        const unsigned c=unsigned((prim>>9)&1);
        const auto frame=state.regs[0x4C+c],zbuf=state.regs[0x4E+c],offset=state.regs[0x18+c],scissor=state.regs[0x40+c],test=state.regs[0x47+c];
        const unsigned fbp=unsigned(frame&511),width=unsigned((frame>>16)&63),psm=unsigned((frame>>24)&63);
        const unsigned zbp=unsigned(zbuf&511),zpsm=unsigned((zbuf>>24)&15);const bool zmsk=(zbuf>>32)&1;
        // Alpha and destination-alpha tests off; depth test off or ALWAYS (other modes later).
        if(!width || (psm!=0 && psm!=1) || (test&1) || (test&0x4000) || ((test>>16)&1 && ((test>>17)&3)!=1) || (!zmsk && zpsm!=0 && zpsm!=1))reject();
        const unsigned ox=unsigned(offset&0xFFFF),oy=unsigned((offset>>32)&0xFFFF);
        auto x=[&](unsigned v){return int(v&0xFFFF)-int(ox);};auto y=[&](unsigned v){return int((v>>16)&0xFFFF)-int(oy);};
        const auto v0=state.vertex_xyz[0],v1=state.vertex_xyz[1];
        int x0=(std::min(x(unsigned(v0)),x(unsigned(v1)))+15)>>4,x1=(std::max(x(unsigned(v0)),x(unsigned(v1)))+15)>>4;
        int y0=(std::min(y(unsigned(v0)),y(unsigned(v1)))+15)>>4,y1=(std::max(y(unsigned(v0)),y(unsigned(v1)))+15)>>4;
        x0=std::max(x0,int(scissor&2047));y0=std::max(y0,int((scissor>>32)&2047));
        x1=std::min(x1,int((scissor>>16)&2047)+1);y1=std::min(y1,int((scissor>>48)&2047)+1);
        if(x1>int(width*64) || y1>2048)reject();
        if(!apply)return;
        const std::uint32_t color=std::uint32_t(state.vertex_rgbaq[1]),mask=std::uint32_t(frame>>32)|(psm==1?0xFF000000u:0u);
        const std::uint32_t z=std::uint32_t(v1>>32)&(zpsm==1?0xFFFFFFu:0xFFFFFFFFu),zmask=zpsm==1?0xFFFFFFu:0xFFFFFFFFu;
        for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
            auto& w=vram_[address(fbp,width,unsigned(px),unsigned(py),false)];
            w.value=(w.value&mask)|(color&~mask);w.known|=~mask;
            if(!zmsk) {auto& d=vram_[address(zbp,width,unsigned(px),unsigned(py),true)];d.value=(d.value&~zmask)|z;d.known|=zmask;}
            ++frame_pixels;
        }
        ++frame_sprites;
    }
public:
    std::array<std::uint64_t,2> tex0{};
    std::uint64_t pabe=0;
    unsigned configuration_packets=0;
    unsigned packets=0,sprites=0;
    std::uint64_t pixels=0;
    std::array<std::uint64_t,14> registers{};
private:
    bool drawing_reset_=false;
    std::vector<Word> vram_;
};
