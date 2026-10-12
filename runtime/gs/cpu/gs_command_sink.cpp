#include <map>
#include <string>
#include <cstdio>
#include <algorithm>
#include "gs_command_sink.h"

// GsCommandSink implementation: GIF parsing, register writes, image/CLUT transfers and the
// sprite/triangle rasterizer. Kept out of the header so rendering changes rebuild one translation
// unit instead of every generated unit that includes ps2_runtime.h.

struct GsCommandSink::Draw {
    std::uint64_t frame=0,texture0=0;unsigned fbp=0,width=0,psm=0,zbp=0,zpsm=0,ox=0,oy=0,tw=0,th=0,tfx=0,wms=0,wmt=0,sa=0,sb=0,sc=0,sd=0;
    unsigned ztst=1; // 0 NEVER, 1 ALWAYS (or ZTE off), 2 GEQUAL, 3 GREATER.
    bool zmsk=false,tme=false,abe=false,tcc=false,linear=false,colclamp=false,pabe_on=false;int fix=0;
    int sx0=0,sy0=0,sx1=0,sy1=0; // Scissor and buffer limits: pixels in [sx0,sx1) x [sy0,sy1).
    std::uint32_t mask=0,zmask=0;
    unsigned tbp=0,tbw=0,csa=0;bool t8=false; // Texture buffer fields decoded once per primitive.
};

struct GsCommandSink::Rgba {int r,g,b,a;};

void GsCommandSink::consume(std::span<const std::uint8_t> packet) {
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
    if(on_accepted)on_accepted(packet);
}

void GsCommandSink::consumeConfiguration(std::span<const std::uint8_t> packet) {
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
    if(on_accepted)on_accepted(packet);
}

void GsCommandSink::transferAll(std::span<const std::span<const std::uint8_t>> group) {
    for(const auto& data:group)if(data.empty() || data.size()%16)throw std::runtime_error("Unsupported GIF transfer size");
    // Rollback: register state and CLUT are copied; VRAM pages are saved on first write (writable).
    const auto saved_state=state;const auto saved_clut=clut_;
    logging_=true;
    const std::array<std::uint64_t,7> saved_counts{transfers,frame_sprites,register_writes,frame_pixels,image_pixels,clut_loads,frame_triangles};
    try {for(const auto& data:group){run(data,true);++transfers;}endTransaction(false);}
    catch(...) {
        endTransaction(true);
        state=saved_state;clut_=saved_clut;
        transfers=unsigned(saved_counts[0]);frame_sprites=unsigned(saved_counts[1]);register_writes=unsigned(saved_counts[2]);
        frame_pixels=saved_counts[3];image_pixels=saved_counts[4];clut_loads=unsigned(saved_counts[5]);frame_triangles=unsigned(saved_counts[6]);throw;
    }
    if(on_accepted)for(const auto& data:group)on_accepted(data);
}

void GsCommandSink::run(std::span<const std::uint8_t> data,bool apply) {
    auto read=[&](std::size_t offset) {std::uint64_t v;std::memcpy(&v,data.data()+offset,8);return v;};
    std::size_t at=0;
    // The GIF path keeps its tag across DMA transfers: IMAGE data left over from an earlier transfer comes first.
    if(state.image_qwords_left) {
        const std::size_t take=std::min<std::size_t>(state.image_qwords_left,data.size()/16);
        image(data.subspan(0,take*16));at=take*16;state.image_qwords_left-=unsigned(take);
    }
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
                // A+D has no REGLIST meaning; that descriptor's data is not a register write.
                if(d!=0xE && d!=0xF)write(d,read(at+w*8),apply);
            }
            at+=((words+1)/2)*16;
        } else {
            const std::size_t take=std::min<std::size_t>(nloop,(data.size()-at)/16);
            image(data.subspan(at,take*16));at+=take*16;state.image_qwords_left=nloop-unsigned(take);
        }
        // EOP ends this GIF packet; any further data in the transfer starts the next packet.
    }
}

void GsCommandSink::image(std::span<const std::uint8_t> data) {
    // Host->local PSMCT32 pixels in transfer order (left to right, top to bottom).
    auto& t=state;
    if(!t.trx)throw std::runtime_error("GIF IMAGE without an active host-to-local transfer");
    for(std::size_t i=0;i<data.size();i+=4) {
        if(t.trx_y>=t.trx_h)throw std::runtime_error("GIF IMAGE data exceeds the transfer region");
        std::uint32_t v;std::memcpy(&v,data.data()+i,4);
        writable(ct32Word(t.trx_bp,t.trx_bw,t.trx_x0+t.trx_x,t.trx_y0+t.trx_y))={v,UINT32_MAX};++image_pixels;
        if(++t.trx_x==t.trx_w) {t.trx_x=0;++t.trx_y;}
    }
    if(t.trx_y==t.trx_h)t.trx=false;
}

void GsCommandSink::startTransfer(std::uint64_t dir) {
    const auto bitblt=state.regs[0x50],pos=state.regs[0x51],reg=state.regs[0x52];
    const unsigned dbp=unsigned((bitblt>>32)&0x3FFF),dbw=unsigned((bitblt>>48)&63),dpsm=unsigned((bitblt>>56)&63);
    const unsigned dx=unsigned((pos>>32)&0x7FF),dy=unsigned((pos>>48)&0x7FF),order=unsigned((pos>>59)&3);
    const unsigned w=unsigned(reg&0xFFF),h=unsigned((reg>>32)&0xFFF);
    // Host->local, CT32 destination, upper-left order, inside the buffer (one page when DBW is 0).
    // A TRXDIR write starts a new transfer; an incomplete earlier one is abandoned, as on the GS.
    if((dir&3)!=0 || dpsm!=0 || order || !w || !h
        || (dbw ? dx+w>dbw*64u : (dx+w>64 || dy+h>32)) || dy+h>2048)
        throw std::runtime_error("Unsupported GS image transfer: dir "+std::to_string(dir&3)+" dpsm "+std::to_string(dpsm)
            +" order "+std::to_string(order)+" "+std::to_string(w)+"x"+std::to_string(h)+" at "+std::to_string(dx)+","+std::to_string(dy)+" dbw "+std::to_string(dbw));
    state.trx=true;state.trx_bp=dbp;state.trx_bw=dbw;state.trx_x0=dx;state.trx_y0=dy;state.trx_w=w;state.trx_h=h;state.trx_x=state.trx_y=0;
}

void GsCommandSink::loadClut(std::uint64_t texture0) {
    // CLD 1: load at this TEX0 write. CSM1 with CT32 entries, from a 64-pixel-wide buffer at CBP:
    // PSMT4 loads 16 entries (8x2) at CSA*16; PSMT8 loads 256 entries (16x16, index bits 3 and 4
    // swapped in the layout).
    const unsigned psm=unsigned((texture0>>20)&63),cbp=unsigned((texture0>>37)&0x3FFF),cpsm=unsigned((texture0>>51)&15);
    const unsigned csm=unsigned((texture0>>55)&1),csa=unsigned((texture0>>56)&31);
    if((psm!=0x14 && psm!=0x13) || cpsm!=0 || csm || csa>15 || (psm==0x13 && csa))
        throw std::runtime_error("Unsupported GS CLUT load: psm "+std::to_string(psm)+" cpsm "+std::to_string(cpsm)+" csm "+std::to_string(csm)+" csa "+std::to_string(csa)+" cbp "+std::to_string(cbp));
    if(psm==0x14) {
        for(unsigned i=0;i<16;++i) {
            const auto& w=vram_[ct32Word(cbp,1,i%8,i/8)];
            clut_.color[csa*16+i]=w.value;clut_.known[csa*16+i]=w.known==UINT32_MAX;
        }
    } else {
        // Entry offsets of the 16x16 CT32 layout relative to CBP, computed once.
        static const auto offsets=[]{std::array<std::size_t,256> o{};for(unsigned i=0;i<256;++i){const unsigned s=(i&~0x18u)|((i&0x08u)<<1)|((i&0x10u)>>1);o[i]=ct32Word(0,1,s%16,s/16);}return o;}();
        for(unsigned i=0;i<256;++i) {
            const auto& w=vram_[(std::size_t(cbp)*64+offsets[i])%(1024*1024)];
            clut_.color[i]=w.value;clut_.known[i]=w.known==UINT32_MAX;
        }
    }
    ++clut_loads;
}

void GsCommandSink::packed(unsigned d,std::uint64_t lo,std::uint64_t hi,bool apply) {
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

void GsCommandSink::write(unsigned reg,std::uint64_t value,bool apply) {
    if(reg>=state.regs.size() || (reg>0x0F && reg<0x14) || reg==0x54)
        throw std::runtime_error("Unsupported GS register write");
    state.regs[reg]=value;if(apply)++register_writes;
    if(reg==0x53) {startTransfer(value);return;}
    if(reg==0x06 || reg==0x07) {
        const unsigned cld=unsigned((value>>61)&7);
        if(cld==1)loadClut(value);else if(cld)throw std::runtime_error("Unsupported GS CLUT load control");
        return;
    }
    if(reg==0x00)state.vertices=0;
    if(reg!=0x04 && reg!=0x05 && reg!=0x0C && reg!=0x0D)return;
    const auto prim=(state.regs[0x1A]&1)?state.regs[0x00]:state.regs[0x1B];
    // Vertex queue: sprites and triangles restart after each primitive; a triangle strip keeps the
    // last two vertices and a fan keeps the first and the last. XYZ3/XYZF3 queue without drawing.
    const unsigned type=unsigned(state.regs[0x00]&7),needed=type==6?2:(type>=3 && type<=5)?3:0;
    if(!needed)throw std::runtime_error("Unsupported GS primitive type "+std::to_string(type));
    if(state.vertices>=needed) {
        if(type==4) {for(unsigned k=0;k<2;++k){state.vertex_xyz[k]=state.vertex_xyz[k+1];state.vertex_rgbaq[k]=state.vertex_rgbaq[k+1];state.vertex_uv[k]=state.vertex_uv[k+1];}state.vertices=2;}
        else if(type==5) {state.vertex_xyz[1]=state.vertex_xyz[2];state.vertex_rgbaq[1]=state.vertex_rgbaq[2];state.vertex_uv[1]=state.vertex_uv[2];state.vertices=2;}
        else state.vertices=0;
    }
    state.vertex_xyz[state.vertices]=value;state.vertex_rgbaq[state.vertices]=state.regs[0x01];state.vertex_uv[state.vertices]=state.regs[0x03];++state.vertices;
    if(state.vertices<needed || reg==0x0C || reg==0x0D)return;
    if(type==6) {sprite(prim,apply);state.vertices=0;}
    else triangle(prim,apply);
}

inline GsCommandSink::Rgba GsCommandSink::unpack(std::uint32_t c) {
    return {int(c&255),int((c>>8)&255),int((c>>16)&255),int(c>>24)};
}

inline std::uint32_t GsCommandSink::texel(const Draw& d,int u,int v) const {
    const unsigned tbp=d.tbp,tbw=d.tbw,csa=d.csa;const bool t8=d.t8;
    const auto n=t8?t8Byte(tbp,tbw,unsigned(u),unsigned(v)):t4Nibble(tbp,tbw,unsigned(u),unsigned(v));
    const auto& w=vram_[t8?n/4:n/8];const unsigned shift=t8?unsigned(n%4)*8:unsigned(n%8)*4,mask=t8?255u:15u;
    if(((w.known>>shift)&mask)!=mask)throw std::runtime_error("GS texture reads unwritten VRAM at texel "+std::to_string(u)+","+std::to_string(v)+" (TBP "+std::to_string(tbp)+", TBW "+std::to_string(tbw)+", word "+std::to_string(t8?n/4:n/8)+")");
    const unsigned index=t8?((w.value>>shift)&255):csa*16+((w.value>>shift)&15);
    if(!clut_.known[index])throw std::runtime_error("GS texture reads an unloaded CLUT entry");
    return clut_.color[index];
}

GsCommandSink::Draw GsCommandSink::prepare(std::uint64_t prim) const {
    auto reject=[](const char* what){throw std::runtime_error(std::string("Unsupported GS drawing state: ")+what);};
    // TME (with FST UVs) and ABE are supported; FGE/AA1 are not.
    Draw d;d.tme=(prim>>4)&1;d.abe=(prim>>6)&1;const bool fst=(prim>>8)&1;
    if(prim&0x20)reject("fogging (FGE)");
    if(prim&0x80)reject("antialiasing (AA1)");
    if(d.tme && !fst)reject("STQ texture coordinates (FST 0)");
    const unsigned c=unsigned((prim>>9)&1);
    d.frame=state.regs[0x4C+c];const auto zbuf=state.regs[0x4E+c],offset=state.regs[0x18+c],scissor=state.regs[0x40+c],test=state.regs[0x47+c];
    d.fbp=unsigned(d.frame&511);d.width=unsigned((d.frame>>16)&63);d.psm=unsigned((d.frame>>24)&63);
    d.zbp=unsigned(zbuf&511);d.zpsm=unsigned((zbuf>>24)&15);d.zmsk=(zbuf>>32)&1;
    // Alpha and destination-alpha tests off; depth test off or ALWAYS; no dither or FBA.
    if(!d.width)reject("frame width 0");
    if(d.psm!=0 && d.psm!=1)reject("frame pixel format");
    if(test&1)reject("alpha test");
    if(test&0x4000)reject("destination alpha test");
    d.ztst=(test>>16)&1?unsigned((test>>17)&3):1u;
    if((!d.zmsk || d.ztst>=2) && d.zpsm!=0 && d.zpsm!=1)reject("depth buffer format");
    if(state.regs[0x45]&1)reject("dithering");
    if(state.regs[0x4A+c]&1)reject("FBA");
    d.ox=unsigned(offset&0xFFFF);d.oy=unsigned((offset>>32)&0xFFFF);
    d.sx0=int(scissor&2047);d.sy0=int((scissor>>32)&2047);
    d.sx1=std::min(int((scissor>>16)&2047)+1,int(d.width*64));d.sy1=std::min(int((scissor>>48)&2047)+1,2048);
    // Texture state: PSMT4 or PSMT8 with a loaded CT32 CLUT, modulate/decal, no mipmapping, one filter for
    // magnification and minification (so the LOD cannot change the result), REPEAT/CLAMP wrap.
    d.texture0=state.regs[0x06+c];const auto tex1=state.regs[0x14+c],clamp=state.regs[0x08+c];
    d.tw=unsigned((d.texture0>>26)&15);d.th=unsigned((d.texture0>>30)&15);d.tfx=unsigned((d.texture0>>35)&3);
    d.tbp=unsigned(d.texture0&0x3FFF);d.tbw=unsigned((d.texture0>>14)&63);d.csa=unsigned((d.texture0>>56)&31);d.t8=((d.texture0>>20)&63)==0x13;
    d.tcc=(d.texture0>>34)&1;d.linear=(tex1>>5)&1;d.wms=unsigned(clamp&3);d.wmt=unsigned((clamp>>2)&3);
    if(d.tme) {
        if(((d.texture0>>20)&63)!=0x14 && ((d.texture0>>20)&63)!=0x13)reject("texture pixel format");
        if(d.tw>10 || d.th>10)reject("texture size");
        if(d.tfx>1)reject("texture function HIGHLIGHT/HIGHLIGHT2");
        if(((tex1>>2)&7) || ((tex1>>6)&7)!=(d.linear?1u:0u))reject("mipmapping or mixed filters");
        if(d.wms>1 || d.wmt>1)reject("REGION texture wrap");
    }
    // Blending: ((A-B)*C>>7)+D with A/B/D in {Cs,Cd,0} and C in {As,Ad,FIX}; COLCLAMP clamps.
    const auto alpha=state.regs[0x42+c];
    d.sa=unsigned(alpha&3);d.sb=unsigned((alpha>>2)&3);d.sc=unsigned((alpha>>4)&3);d.sd=unsigned((alpha>>6)&3);
    d.fix=int((alpha>>32)&255);d.colclamp=state.regs[0x46]&1;d.pabe_on=state.regs[0x49]&1;
    if(d.abe && (d.sa==3 || d.sb==3 || d.sc==3 || d.sd==3))reject("reserved blend selector");
    d.mask=std::uint32_t(d.frame>>32)|(d.psm==1?0xFF000000u:0u);d.zmask=d.zpsm==1?0xFFFFFFu:0xFFFFFFFFu;
    return d;
}

inline GsCommandSink::Rgba GsCommandSink::sample(const Draw& d,int u,int v) const {
    // u,v in 1/16 texels at the pixel.
    auto wrap=[](int t,unsigned size_log2,unsigned mode){const int size=1<<size_log2;return mode?std::clamp(t,0,size-1):(t&(size-1));};
    if(!d.linear)return unpack(texel(d,wrap(u>>4,d.tw,d.wms),wrap(v>>4,d.th,d.wmt)));
    const int uu=u-8,vv=v-8,fu=uu&15,fv=vv&15,iu=uu>>4,iv=vv>>4;
    const auto c00=unpack(texel(d,wrap(iu,d.tw,d.wms),wrap(iv,d.th,d.wmt))),c10=unpack(texel(d,wrap(iu+1,d.tw,d.wms),wrap(iv,d.th,d.wmt)));
    const auto c01=unpack(texel(d,wrap(iu,d.tw,d.wms),wrap(iv+1,d.th,d.wmt))),c11=unpack(texel(d,wrap(iu+1,d.tw,d.wms),wrap(iv+1,d.th,d.wmt)));
    auto mix=[&](int a,int b,int c2,int e){return (a*(16-fu)*(16-fv)+b*fu*(16-fv)+c2*(16-fu)*fv+e*fu*fv)>>8;};
    return {mix(c00.r,c10.r,c01.r,c11.r),mix(c00.g,c10.g,c01.g,c11.g),mix(c00.b,c10.b,c01.b,c11.b),mix(c00.a,c10.a,c01.a,c11.a)};
}

void GsCommandSink::plot(const Draw& d,int px,int py,Rgba vertex,int u,int v,std::uint32_t z) {
    // Depth test before any write: NEVER draws nothing; GEQUAL/GREATER compare with the stored Z.
    if(d.ztst==0)return;
    if(d.ztst>=2) {
        const auto zw=word(address(d.zbp,d.width,unsigned(px),unsigned(py),true));
        if((zw.known&d.zmask)!=d.zmask)throw std::runtime_error("GS depth test reads unwritten depth buffer");
        const auto stored=zw.value&d.zmask,incoming=z&d.zmask;
        if(d.ztst==2?incoming<stored:incoming<=stored)return;
    }
    Rgba s=vertex;
    if(d.tme) {
        const auto t=sample(d,u,v);
        if(d.tfx==0)s={std::min(255,(t.r*vertex.r)>>7),std::min(255,(t.g*vertex.g)>>7),std::min(255,(t.b*vertex.b)>>7),d.tcc?std::min(255,(t.a*vertex.a)>>7):vertex.a};
        else s={t.r,t.g,t.b,d.tcc?t.a:vertex.a};
    }
    auto& w=writable(ct32Word(d.fbp*32,d.width,unsigned(px),unsigned(py)));
    if(d.abe && !(d.pabe_on && s.a<0x80)) {
        const std::uint32_t needed=((d.sa==1 || d.sb==1 || d.sd==1)?0xFFFFFFu:0u)|(d.sc==1?0xFF000000u:0u);
        if((w.known&needed)!=needed)throw std::runtime_error("GS blend reads unwritten framebuffer");
        const auto dst=unpack(w.value);
        // ((A-B)*C>>7)+D per channel; A, B and D select Cs (0), Cd (1) or 0 (2).
        const int ca=d.sc==0?s.a:d.sc==1?dst.a:d.fix;
        const int cs[3]={s.r,s.g,s.b},cd[3]={dst.r,dst.g,dst.b};int out[3];
        for(unsigned k=0;k<3;++k) {
            const int a=d.sa==0?cs[k]:d.sa==1?cd[k]:0,b=d.sb==0?cs[k]:d.sb==1?cd[k]:0,e=d.sd==0?cs[k]:d.sd==1?cd[k]:0;
            const int r=((a-b)*ca>>7)+e;out[k]=d.colclamp?std::clamp(r,0,255):(r&255);
        }
        s={out[0],out[1],out[2],s.a};
    }
    const std::uint32_t color=std::uint32_t(s.r)|(std::uint32_t(s.g)<<8)|(std::uint32_t(s.b)<<16)|(std::uint32_t(s.a&255)<<24);
    w.value=(w.value&d.mask)|(color&~d.mask);w.known|=~d.mask;
    if(!d.zmsk) {auto& zw=writable(address(d.zbp,d.width,unsigned(px),unsigned(py),true));zw.value=(zw.value&~d.zmask)|(z&d.zmask);zw.known|=d.zmask;}
    ++frame_pixels;
}

void GsCommandSink::sprite(std::uint64_t prim,bool apply) {
    const auto d=prepare(prim);
    auto x=[&](unsigned v){return int(v&0xFFFF)-int(d.ox);};auto y=[&](unsigned v){return int((v>>16)&0xFFFF)-int(d.oy);};
    const auto v0=state.vertex_xyz[0],v1=state.vertex_xyz[1];
    const int X0=x(unsigned(v0)),X1=x(unsigned(v1)),Y0=y(unsigned(v0)),Y1=y(unsigned(v1));
    int x0=(std::min(X0,X1)+15)>>4,x1=(std::max(X0,X1)+15)>>4;
    int y0=(std::min(Y0,Y1)+15)>>4,y1=(std::max(Y0,Y1)+15)>>4;
    // A sprite reaching past the scissored buffer is refused, as before (not clipped silently).
    const auto scissor=state.regs[0x40+unsigned((prim>>9)&1)];
    if(std::min(int((scissor>>16)&2047)+1,x1)>int(d.width*64) || std::min(int((scissor>>48)&2047)+1,y1)>2048)
        throw std::runtime_error("Unsupported GS drawing state: sprite beyond the frame buffer");
    x0=std::max(x0,d.sx0);y0=std::max(y0,d.sy0);x1=std::min(x1,d.sx1);y1=std::min(y1,d.sy1);
    const int U0=int(state.vertex_uv[0]&0x3FFF),V0=int((state.vertex_uv[0]>>16)&0x3FFF);
    const int U1=int(state.vertex_uv[1]&0x3FFF),V1=int((state.vertex_uv[1]>>16)&0x3FFF);
    if(!apply)return;
    if(!rasterize){++frame_sprites;return;}
    const auto vertex=unpack(std::uint32_t(state.vertex_rgbaq[1]));
    const std::uint32_t z=std::uint32_t(v1>>32);
    // U depends only on the column and V only on the row: each is computed once.
    std::vector<int> us(std::size_t(std::max(0,x1-x0)));
    for(int px=x0;px<x1;++px)us[std::size_t(px-x0)]=X1==X0?U0:U0+int((std::int64_t(U1-U0)*(px*16-X0))/(X1-X0));
    for(int py=y0;py<y1;++py) {
        const int v=Y1==Y0?V0:V0+int((std::int64_t(V1-V0)*(py*16-Y0))/(Y1-Y0));
        for(int px=x0;px<x1;++px)plot(d,px,py,vertex,us[std::size_t(px-x0)],v,z);
    }
    ++frame_sprites;
}

void GsCommandSink::triangle(std::uint64_t prim,bool apply) {
    const auto d=prepare(prim);
    const bool gouraud=(prim>>3)&1;
    std::array<std::int64_t,3> X{},Y{};
    for(unsigned k=0;k<3;++k){X[k]=std::int64_t(state.vertex_xyz[k]&0xFFFF)-d.ox;Y[k]=std::int64_t((state.vertex_xyz[k]>>16)&0xFFFF)-d.oy;}
    std::int64_t area=(X[1]-X[0])*(Y[2]-Y[0])-(Y[1]-Y[0])*(X[2]-X[0]);
    if(!area){if(apply)++frame_triangles;return;} // Degenerate: no pixels.
    std::array<unsigned,3> order{0,1,2};if(area<0){std::swap(order[1],order[2]);area=-area;}
    const auto minx=std::min({X[0],X[1],X[2]}),maxx=std::max({X[0],X[1],X[2]}),miny=std::min({Y[0],Y[1],Y[2]}),maxy=std::max({Y[0],Y[1],Y[2]});
    const int x0=std::max(int((minx+15)>>4),d.sx0),x1=std::min(int((maxx>>4)+1),d.sx1);
    const int y0=std::max(int((miny+15)>>4),d.sy0),y1=std::min(int((maxy>>4)+1),d.sy1);
    if(!apply)return;
    if(!rasterize){++frame_triangles;return;}
    // Edge k is opposite vertex order[k]. With positive area (y down), a point is inside when every
    // edge function is >= 0; on an edge (== 0) only top edges (horizontal, above the triangle) and
    // left edges own the pixel, so pixels shared by neighbouring triangles are drawn once.
    struct Edge {std::int64_t a,b,c;bool inclusive;};std::array<Edge,3> edges{};
    for(unsigned k=0;k<3;++k) {
        const auto i=order[(k+1)%3],j=order[(k+2)%3];
        const auto a=Y[i]-Y[j],b=X[j]-X[i];
        edges[k]={a,b,-(a*X[i]+b*Y[i]),(a>0) || (a==0 && b>0)};
    }
    std::array<Rgba,3> col{};std::array<std::int64_t,3> U{},V{},Z{};
    for(unsigned k=0;k<3;++k) {
        col[k]=unpack(std::uint32_t(state.vertex_rgbaq[order[k]]));U[k]=std::int64_t(state.vertex_uv[order[k]]&0x3FFF);
        V[k]=std::int64_t((state.vertex_uv[order[k]]>>16)&0x3FFF);Z[k]=std::int64_t(state.vertex_xyz[order[k]]>>32);
    }
    const auto flat=unpack(std::uint32_t(state.vertex_rgbaq[2]));
    // Fast path: the edge functions and attribute numerators are linear in px, so each row starts from
    // the direct formula and steps by constants. Attributes keep exact floor quotient/remainder pairs,
    // equal to the truncating division below wherever a pixel is inside (numerators are then >= 0).
    // Used only when no numerator can exceed 2^62; otherwise the direct per-pixel formula runs.
    std::int64_t max_attribute=0,max_weight=0;
    for(unsigned k=0;k<3;++k) {
        max_attribute=std::max({max_attribute,Z[k],U[k],V[k],std::int64_t(255)});
        for(const int cx:{x0,x1-1})for(const int cy:{y0,y1-1})
            max_weight=std::max(max_weight,std::abs(edges[k].a*cx*16+edges[k].b*cy*16+edges[k].c));
        max_weight=std::max(max_weight,std::abs(edges[k].a*16));
    }
    if(x0<x1 && y0<y1 && max_weight<=(std::int64_t(1)<<60)/max_attribute) {
        auto floor_div=[&](std::int64_t n){std::int64_t q=n/area;if(n%area<0)--q;return q;};
        struct Lerp {std::int64_t q=0,r=0,dq=0,dr=0;};
        auto start=[&](std::int64_t n,std::int64_t dn){Lerp l;l.q=floor_div(n);l.r=n-l.q*area;l.dq=floor_div(dn);l.dr=dn-l.dq*area;return l;};
        auto step=[&](Lerp& l){l.q+=l.dq;l.r+=l.dr;if(l.r>=area){l.r-=area;++l.q;}};
        const unsigned attributes=gouraud?7u:3u; // Z, U, V, then R, G, B, A when Gouraud.
        std::array<std::array<std::int64_t,3>,7> value{};value[0]=Z;value[1]=U;value[2]=V;
        for(unsigned k=0;k<3;++k){value[3][k]=col[k].r;value[4][k]=col[k].g;value[5][k]=col[k].b;value[6][k]=col[k].a;}
        for(int py=y0;py<y1;++py) {
            std::array<std::int64_t,3> w{},dw{};
            for(unsigned k=0;k<3;++k){w[k]=edges[k].a*x0*16+edges[k].b*py*16+edges[k].c;dw[k]=edges[k].a*16;}
            std::array<Lerp,7> l{};
            for(unsigned n=0;n<attributes;++n)
                l[n]=start(value[n][0]*w[0]+value[n][1]*w[1]+value[n][2]*w[2],value[n][0]*dw[0]+value[n][1]*dw[1]+value[n][2]*dw[2]);
            for(int px=x0;px<x1;++px) {
                bool inside=true;
                for(unsigned k=0;k<3;++k)if(w[k]<0 || (w[k]==0 && !edges[k].inclusive)){inside=false;break;}
                if(inside) {
                    const Rgba vertex=gouraud?Rgba{int(l[3].q),int(l[4].q),int(l[5].q),int(l[6].q)}:flat;
                    plot(d,px,py,vertex,int(l[1].q),int(l[2].q),std::uint32_t(std::clamp<std::int64_t>(l[0].q,0,0xFFFFFFFFll)));
                }
                for(unsigned k=0;k<3;++k)w[k]+=dw[k];
                for(unsigned n=0;n<attributes;++n)step(l[n]);
            }
        }
        ++frame_triangles;return;
    }
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px) {
        std::array<std::int64_t,3> w{};bool inside=true;
        for(unsigned k=0;k<3;++k) {
            w[k]=edges[k].a*px*16+edges[k].b*py*16+edges[k].c;
            if(w[k]<0 || (w[k]==0 && !edges[k].inclusive)){inside=false;break;}
        }
        if(!inside)continue;
        auto lerp=[&](std::int64_t a,std::int64_t b,std::int64_t c){return (a*w[0]+b*w[1]+c*w[2])/area;};
        const Rgba vertex=gouraud?Rgba{int(lerp(col[0].r,col[1].r,col[2].r)),int(lerp(col[0].g,col[1].g,col[2].g)),int(lerp(col[0].b,col[1].b,col[2].b)),int(lerp(col[0].a,col[1].a,col[2].a))}:flat;
        const auto z=std::uint32_t(std::clamp<std::int64_t>(lerp(Z[0],Z[1],Z[2]),0,0xFFFFFFFFll));
        plot(d,px,py,vertex,int(lerp(U[0],U[1],U[2])),int(lerp(V[0],V[1],V[2])),z);
    }
    ++frame_triangles;
}
