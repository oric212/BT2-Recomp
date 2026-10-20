#include "gs/textures/gs_texture_names.h"
#include "gs/cpu/gs_command_sink.h"
#define XXH_INLINE_ALL
#include "xxhash.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr unsigned psm_ct32=0x00,psm_ct24=0x01,psm_t8=0x13,psm_t4=0x14,psm_t8h=0x1B,psm_t4hl=0x24,psm_t4hh=0x2C;
constexpr std::size_t memory_words=1024*1024,max_blocks=16384;

// What naming needs of a texture format: block size, palette entries, whether its bits fill a
// 32-bit pixel (else it is hashed as read out), and the page geometry of its blocks.
struct Format {unsigned block_w,block_h,palette;bool full;unsigned page_w,page_h,blocks_w,blocks_h;};
std::optional<Format> format(unsigned psm) {
    switch(psm) {
    case psm_ct32:return Format{8,8,0,true,64,32,8,4};
    case psm_ct24:return Format{8,8,0,false,64,32,8,4};
    case psm_t8:return Format{16,16,256,true,128,64,8,4};
    case psm_t4:return Format{32,16,16,true,128,128,4,8};
    case psm_t8h:return Format{8,8,256,false,64,32,8,4};
    case psm_t4hl:case psm_t4hh:return Format{8,8,16,false,64,32,8,4};
    default:return std::nullopt;
    }
}
// A block's place in its page (public GS layout, as gs_command_sink.h places it): pages of 8 x 4 blocks
// for the 32-bit and 8-bit formats, 4 x 8 for 4-bit.
unsigned blockInPage(unsigned psm,unsigned bx,unsigned by) {
    if(psm==psm_t4)return (by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2);
    return (bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
}
unsigned log2(unsigned v) {unsigned n=0;while(v>1){v>>=1;++n;}return n;}
}

std::string GsTextureNames::Name::text() const {
    char buffer[64];
    if(has_palette)std::snprintf(buffer,sizeof(buffer),"%llx-%llx-%08x",static_cast<unsigned long long>(texture),static_cast<unsigned long long>(palette),format);
    else std::snprintf(buffer,sizeof(buffer),"%llx-%08x",static_cast<unsigned long long>(texture),format);
    return buffer;
}

void GsTextureNames::reg(unsigned reg,std::uint64_t value) {
    if(reg>=regs_.size())return;
    if(reg==0x16 || reg==0x17) {
        // TEX2: PSM and the CLUT fields of TEX0 (CBP, CPSM, CSM, CSA, CLD).
        constexpr std::uint64_t mask=(std::uint64_t(63)<<20)|(~std::uint64_t(0)<<37);
        const unsigned tex0=reg-0x16+6;
        regs_[reg]=value;reg=tex0;value=(regs_[tex0]&~mask)|(value&mask);
    }
    regs_[reg]=value;
    if(reg==0x53) {startTransfer(value);return;}
    if(reg==0x06 || reg==0x07) {
        // CLD: 1 to 3 load the CLUT (2 and 3 also keep CBP), 4 and 5 load when CBP differs from the kept one.
        const unsigned cld=unsigned((value>>61)&7);const std::uint32_t cbp=std::uint32_t((value>>37)&0x3FFF);
        const bool load=(cld>=1 && cld<=3) || (cld==4 && cbp!=cbp_[0]) || (cld==5 && cbp!=cbp_[1]);
        if(cld==2 || cld==4)cbp_[0]=cbp;
        if(cld==3 || cld==5)cbp_[1]=cbp;
        if(load)loadClut(value);
        return;
    }
    if(reg==0x04 || reg==0x05)kick();
}

void GsTextureNames::startTransfer(std::uint64_t dir) {
    transfer_=false;partial_=0;partial_bytes_=0;
    const unsigned direction=unsigned(dir&3);
    if(direction==2) {++local_copies;all_written_=++generation_;return;}
    if(direction!=0)return;
    const auto bitblt=regs_[0x50],pos=regs_[0x51],size=regs_[0x52];
    psm_=unsigned((bitblt>>56)&63);bp_=unsigned((bitblt>>32)&0x3FFF);bw_=unsigned((bitblt>>48)&63);
    x0_=unsigned((pos>>32)&0x7FF);y0_=unsigned((pos>>48)&0x7FF);w_=unsigned(size&0xFFF);h_=unsigned((size>>32)&0xFFF);x_=y_=0;
    const bool written=psm_==psm_ct32 || psm_==psm_ct24 || psm_==psm_t8 || psm_==psm_t4 || psm_==psm_t8h || psm_==psm_t4hl || psm_==psm_t4hh;
    if(!written || ((pos>>59)&3) || !w_ || !h_) {++unsupported_uploads;all_written_=++generation_;return;}
    transfer_=true;++generation_;
}

void GsTextureNames::image(std::span<const std::uint8_t> data) {
    if(!transfer_)return;
    ++generation_; // A hash taken between two pieces of a transfer is not kept past the next one.
    const unsigned bytes=psm_==psm_ct32?4:psm_==psm_ct24?3:0;
    for(const std::uint8_t b:data) {
        if(!transfer_)return;
        if(bytes) {
            partial_|=std::uint32_t(b)<<(8*partial_bytes_);
            if(++partial_bytes_==bytes) {writePixel(partial_);partial_=0;partial_bytes_=0;}
        } else if(psm_==psm_t4 || psm_==psm_t4hl || psm_==psm_t4hh) {
            writePixel(b&15);
            if(transfer_)writePixel(b>>4);
        } else writePixel(b);
    }
}

void GsTextureNames::writePixel(std::uint32_t value) {
    const unsigned x=(x0_+x_)%2048,y=(y0_+y_)%2048;
    auto& word=[&]()->std::uint32_t& {
        switch(psm_) {
        case psm_t8:return memory_[GsCommandSink::t8Byte(bp_,bw_,x,y)/4];
        case psm_t4:return memory_[GsCommandSink::t4Nibble(bp_,bw_,x,y)/8];
        default:return memory_[GsCommandSink::ct32Word(bp_,bw_,x,y)];
        }
    }();
    switch(psm_) {
    case psm_ct32:word=value;break;
    case psm_ct24:word=(word&0xFF000000u)|(value&0xFFFFFFu);break;
    case psm_t8: {const unsigned shift=unsigned(GsCommandSink::t8Byte(bp_,bw_,x,y)%4)*8;word=(word&~(0xFFu<<shift))|(value<<shift);break;}
    case psm_t4: {const unsigned shift=unsigned(GsCommandSink::t4Nibble(bp_,bw_,x,y)%8)*4;word=(word&~(0xFu<<shift))|(value<<shift);break;}
    case psm_t8h:word=(word&0x00FFFFFFu)|(value<<24);break;
    case psm_t4hl:word=(word&0xF0FFFFFFu)|(value<<24);break;
    case psm_t4hh:word=(word&0x0FFFFFFFu)|(value<<28);break;
    }
    page_written_[(&word-memory_.data())/2048]=generation_;
    ++uploaded_pixels;
    if(++x_==w_) {x_=0;if(++y_==h_)transfer_=false;}
}

void GsTextureNames::loadClut(std::uint64_t tex0) {
    const unsigned psm=unsigned((tex0>>20)&63),cbp=unsigned((tex0>>37)&0x3FFF),cpsm=unsigned((tex0>>51)&15);
    const unsigned csm=unsigned((tex0>>55)&1),csa=unsigned((tex0>>56)&31);
    const auto f=format(psm);
    if(!f || !f->palette)return;
    ++generation_;
    // CSM1 with 32-bit entries: 16 entries as 8 x 2 pixels at CSA's 16; 256 as 16 x 16 pixels with index
    // bits 3 and 4 swapped in the layout, groups of 16 from CSA's on.
    if(csm || (cpsm!=0 && cpsm!=1)) {++unsupported_cluts;return;}
    auto put=[&](unsigned entry,std::uint32_t value){clut_[entry]=std::uint16_t(value);clut_[entry+256]=std::uint16_t(value>>16);};
    if(f->palette==16) {
        for(unsigned k=0;k<16;++k)put((csa&15)*16+k,memory_[GsCommandSink::ct32Word(cbp,1,k%8,k/8)]);
    } else {
        for(unsigned i=(csa&15)*16;i<256;++i) {
            const unsigned s=(i&~0x18u)|((i&0x08u)<<1)|((i&0x10u)>>1);
            put(i,memory_[GsCommandSink::ct32Word(cbp,1,s%16,s/16)]);
        }
    }
}

void GsTextureNames::kick() {
    const auto prim=(regs_[0x1A]&1)?regs_[0x00]:regs_[0x1B];
    if(!((prim>>4)&1))return; // TME
    const unsigned context=unsigned((prim>>9)&1);
    const auto tex0=regs_[6+context]&~(std::uint64_t(7)<<61),texa=regs_[0x3B];
    if(named_generation_[context]==generation_ && named_tex0_[context]==tex0 && named_texa_[context]==texa)return;
    named_generation_[context]=generation_;named_tex0_[context]=tex0;named_texa_[context]=texa;
    if(const auto n=name(tex0,texa))++drawn[n->text()];
}

std::optional<GsTextureNames::Name> GsTextureNames::name(std::uint64_t tex0,std::uint64_t texa) {
    const unsigned psm=unsigned((tex0>>20)&63);
    const unsigned tw=unsigned((tex0>>26)&15),th=unsigned((tex0>>30)&15);
    const unsigned cpsm=unsigned((tex0>>51)&15),csa=unsigned((tex0>>56)&31);
    const auto f=format(psm);
    if(!f || tw>10 || th>10 || (f->palette && cpsm!=0 && cpsm!=1)) {++unsupported_textures;++unsupported_formats[psm];return std::nullopt;}
    const unsigned ta0=unsigned(texa&0xFF),aem=unsigned((texa>>15)&1),ta1=unsigned((texa>>32)&0xFF);
    Name n;
    n.texture=textureHash(tex0,texa);
    n.format=psm|(tw<<6)|(th<<10);
    if(!f->palette && psm!=psm_ct32)n.format|=(ta0<<15)|(aem<<23)|(ta1<<24);
    if(f->palette) {
        std::array<std::uint32_t,256> entries{};
        auto entry=[&](unsigned i){return std::uint32_t(clut_[i])|(std::uint32_t(clut_[i+256])<<16);};
        if(f->palette==16)for(unsigned k=0;k<16;++k)entries[k]=entry((csa&15)*16+k);
        else for(unsigned i=0;i<256;i+=16) {
            // Groups past the last one clamp to it instead of wrapping.
            const unsigned from=std::min(i+(csa&15)*16,240u);
            for(unsigned k=0;k<16;++k)entries[i+k]=entry(from+k);
        }
        n.palette=XXH3_64bits(entries.data(),std::size_t(f->palette)*4);n.has_palette=true;
    }
    return n;
}

std::uint64_t GsTextureNames::textureHash(std::uint64_t tex0,std::uint64_t texa) {
    const unsigned psm=unsigned((tex0>>20)&63),tbp=unsigned(tex0&0x3FFF),tbw=unsigned((tex0>>14)&63);
    const unsigned tw=unsigned((tex0>>26)&15),th=unsigned((tex0>>30)&15);
    const auto f=*format(psm);
    const unsigned width=1u<<tw,height=1u<<th;
    const unsigned ta0=unsigned(texa&0xFF),aem=unsigned((texa>>15)&1);
    // The pages the texture lies in: rows of pages TBW apart (half for 8-bit and 4-bit), each as many
    // pages across as the width needs plus one when TBP is not at a page's start.
    const unsigned pages_across=tbw>>(log2(f.page_w)-6);
    auto pages=[&](auto&& each) {
        const unsigned rows=(height+f.page_h-1)/f.page_h,across=(width+f.page_w-1)/f.page_w+(tbp%32?1:0);
        for(unsigned r=0;r<rows;++r)for(unsigned c=0;c<across;++c)each((tbp/32+r*std::max(pages_across,1u)+c)%512);
    };
    const auto key=std::make_pair(tex0&0x3FFFFFFFFull,psm==psm_ct24?texa&0x80FFull:0ull);
    if(auto found=hashed_.find(key);found!=hashed_.end() && found->second.generation>=all_written_) {
        bool clean=true;
        pages([&](unsigned page){if(page_written_[page]>found->second.generation)clean=false;});
        if(clean)return found->second.hash;
    }
    XXH3_state_t state;XXH3_64bits_reset(&state);
    if(width<f.block_w || height<f.block_h || !f.full) {
        // Read out row by row: the index for palette formats, RGBA otherwise.
        std::vector<std::uint8_t> row(std::size_t(width)*(f.palette?1:4));
        for(unsigned y=0;y<height;++y) {
            for(unsigned x=0;x<width;++x) {
                if(f.palette) {
                    std::uint8_t index;
                    switch(psm) {
                    case psm_t8:index=std::uint8_t(memory_[GsCommandSink::t8Byte(tbp,tbw,x,y)/4]>>(GsCommandSink::t8Byte(tbp,tbw,x,y)%4*8));break;
                    case psm_t4:index=std::uint8_t((memory_[GsCommandSink::t4Nibble(tbp,tbw,x,y)/8]>>(GsCommandSink::t4Nibble(tbp,tbw,x,y)%8*4))&15);break;
                    case psm_t8h:index=std::uint8_t(memory_[GsCommandSink::ct32Word(tbp,tbw,x,y)]>>24);break;
                    case psm_t4hl:index=std::uint8_t((memory_[GsCommandSink::ct32Word(tbp,tbw,x,y)]>>24)&15);break;
                    default:index=std::uint8_t(memory_[GsCommandSink::ct32Word(tbp,tbw,x,y)]>>28);break;
                    }
                    row[x]=index;
                } else {
                    std::uint32_t pixel=memory_[GsCommandSink::ct32Word(tbp,tbw,x,y)];
                    if(psm==psm_ct24) {const std::uint32_t rgb=pixel&0xFFFFFFu;pixel=rgb|((aem && !rgb)?0u:std::uint32_t(ta0)<<24);}
                    std::memcpy(&row[std::size_t(x)*4],&pixel,4);
                }
            }
            XXH3_64bits_update(&state,row.data(),row.size());
        }
    } else {
        // Whole blocks in block order: rows of blocks, each left to right; pages follow each other across
        // and TBW pages (half of it for the 8-bit and 4-bit formats) down.
        for(unsigned by=0;by<height/f.block_h;++by)
            for(unsigned bx=0;bx<width/f.block_w;++bx) {
                const std::size_t block=(tbp+(by/f.blocks_h)*32*pages_across+(bx/f.blocks_w)*32
                    +blockInPage(psm,bx%f.blocks_w,by%f.blocks_h))%max_blocks;
                XXH3_64bits_update(&state,&memory_[block*64],256);
            }
    }
    const auto hash=XXH3_64bits_digest(&state);
    hashed_[key]={hash,generation_};
    return hash;
}
