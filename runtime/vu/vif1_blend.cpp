#include "vu/vif1_blend.h"
#include <cstring>

std::optional<std::vector<Vif1Unpack>> vif1Unpacks(std::span<const std::uint32_t> words) {
    std::vector<Vif1Unpack> out;
    unsigned wl=1,cl=1;
    std::size_t i=0;
    while(i<words.size()) {
        const std::uint32_t code=words[i++];
        const unsigned cmd=(code>>24)&0x7F,imm=code&0xFFFF,num=(code>>16)&0xFF;
        std::size_t data=0;
        if(cmd==0x01) {wl=(imm>>8)&0xFF;cl=imm&0xFF;}                    // STCYCL
        else if(cmd==0x20)data=1;                                          // STMASK
        else if(cmd==0x30 || cmd==0x31)data=4;                             // STROW, STCOL
        else if(cmd==0x4A)data=std::size_t(num?num:256)*2;                 // MPG
        else if(cmd==0x50 || cmd==0x51)data=std::size_t(imm?imm:65536)*4; // DIRECT, DIRECTHL
        else if(cmd>=0x60) {                                               // UNPACK
            const unsigned vn=(cmd>>2)&3,vl=cmd&3,n=num?num:256;
            const std::size_t elements=wl<=cl?n:std::size_t(cl)*(n/wl)+std::min(n%wl,cl);
            if(vl==3 && vn!=3)return std::nullopt; // V*-5 exists only as V4-5.
            data=(std::size_t(32>>vl)*(vn+1)*elements+31)/32;
            if(data>words.size()-i)return std::nullopt;
            out.push_back({i,data,cmd&0xF});
        } else if(cmd>0x17 && cmd!=0x20 && cmd!=0x30 && cmd!=0x31)return std::nullopt; // Not a code with a known size.
        if(data>words.size()-i)return std::nullopt;
        i+=data;
    }
    return out;
}

namespace {
bool normal(std::uint32_t word) {const unsigned exponent=(word>>23)&0xFF;return exponent && exponent!=0xFF;}
}

std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,std::size_t* blended) {
    const auto a=vif1Unpacks(older),b=vif1Unpacks(newer);
    if(!a || !b || a->size()!=b->size())return std::nullopt;
    std::vector<std::uint32_t> out(newer.begin(),newer.end());
    std::size_t changed=0;
    for(std::size_t k=0;k<b->size();++k) {
        const auto& x=(*a)[k];const auto& y=(*b)[k];
        if(x.format!=y.format || x.words!=y.words)return std::nullopt;
        if(y.format!=0xC)continue; // V4-32
        for(std::size_t w=0;w<y.words;++w) {
            const std::uint32_t p=older[x.start+w],q=newer[y.start+w];
            if(p==q || !normal(p) || !normal(q))continue;
            float fp,fq;std::memcpy(&fp,&p,4);std::memcpy(&fq,&q,4);
            const float mid=fp+t*(fq-fp);
            std::memcpy(&out[y.start+w],&mid,4);++changed;
        }
    }
    if(blended)*blended=changed;
    return out;
}
