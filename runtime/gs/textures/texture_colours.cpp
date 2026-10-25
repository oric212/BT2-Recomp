#include "gs/textures/texture_colours.h"
#include <cmath>
#include <cstring>

void TextureColours::add(unsigned cell,float r,float g,float b,float alpha,float count) {
    const float w=alpha*count;
    rgb[cell][0]+=r*w;rgb[cell][1]+=g*w;rgb[cell][2]+=b*w;
    cover[cell]+=w;pixels_[cell]+=count;
}

void TextureColours::finish() {
    for(unsigned c=0;c<cells;++c) {
        if(cover[c]>0)for(auto& v:rgb[c])v/=cover[c];
        cover[c]=pixels_[c]>0?cover[c]/pixels_[c]:0;
        pixels_[c]=0;
    }
}

std::optional<float> TextureColours::distance(const TextureColours& a,const TextureColours& b) {
    constexpr float covered=1.0f/16; // A cell almost all transparent says little about its colour.
    float sum=0;unsigned n=0;
    for(unsigned c=0;c<cells;++c) {
        if(a.cover[c]<covered || b.cover[c]<covered)continue;
        for(unsigned k=0;k<3;++k) {const float d=a.rgb[c][k]-b.rgb[c][k];sum+=d*d;}
        n+=3;
    }
    if(!n)return std::nullopt;
    return std::sqrt(sum/float(n));
}

namespace {
// A block's 16 pixels as RGBA, alpha 0 to 255 (the public BC1 to BC3 layouts).
void colourBlock(const std::uint8_t* b,bool four_colours_always,std::array<std::array<std::uint8_t,4>,16>& out) {
    std::uint16_t c0,c1;std::memcpy(&c0,b,2);std::memcpy(&c1,b+2,2);
    std::uint32_t indices;std::memcpy(&indices,b+4,4);
    const auto expand=[](std::uint16_t c){return std::array<int,3>{((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};};
    const auto e0=expand(c0),e1=expand(c1);
    std::array<std::array<std::uint8_t,4>,4> palette{};
    const bool four=four_colours_always || c0>c1;
    for(int k=0;k<3;++k) {
        palette[0][k]=std::uint8_t(e0[k]);palette[1][k]=std::uint8_t(e1[k]);
        palette[2][k]=std::uint8_t(four?(2*e0[k]+e1[k])/3:(e0[k]+e1[k])/2);
        palette[3][k]=std::uint8_t(four?(e0[k]+2*e1[k])/3:0);
    }
    palette[0][3]=palette[1][3]=palette[2][3]=255;palette[3][3]=four?255:0;
    for(unsigned p=0;p<16;++p)out[p]=palette[(indices>>(2*p))&3];
}
}

std::optional<TextureColours> TextureColours::ofBlocks(unsigned bc,unsigned width,unsigned height,std::span<const std::uint8_t> blocks) {
    if(bc<1 || bc>3 || width<4 || height<4)return std::nullopt;
    const unsigned block_bytes=bc==1?8:16,across=width/4,down=height/4;
    if(blocks.size()<std::size_t(across)*down*block_bytes)return std::nullopt;
    TextureColours colours;
    std::array<std::array<std::uint8_t,4>,16> pixels;
    for(unsigned by=0;by<down;++by)for(unsigned bx=0;bx<across;++bx) {
        const std::uint8_t* b=blocks.data()+(std::size_t(by)*across+bx)*block_bytes;
        colourBlock(bc==1?b:b+8,bc!=1,pixels);
        if(bc==2)for(unsigned p=0;p<16;++p)pixels[p][3]=std::uint8_t(((b[p/2]>>(p%2*4))&15)*17);
        if(bc==3) {
            const int a0=b[0],a1=b[1];
            std::uint64_t bits=0;std::memcpy(&bits,b+2,6);
            for(unsigned p=0;p<16;++p) {
                const int i=int((bits>>(3*p))&7);
                int a;
                if(i<2)a=i?a1:a0;
                else if(a0>a1)a=((8-i)*a0+(i-1)*a1)/7;
                else a=i==6?0:i==7?255:((6-i)*a0+(i-1)*a1)/5;
                pixels[p][3]=std::uint8_t(a);
            }
        }
        for(unsigned p=0;p<16;++p) {
            const unsigned x=bx*4+p%4,y=by*4+p/4;
            const unsigned cell=(y*side/height)*side+x*side/width;
            colours.add(cell,pixels[p][0],pixels[p][1],pixels[p][2],pixels[p][3]/255.0f,1);
        }
    }
    colours.finish();
    return colours;
}
