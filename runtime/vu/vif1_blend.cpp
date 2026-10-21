#include "vu/vif1_blend.h"
#include <algorithm>
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
            out.push_back({i,data,cmd&0xF,code});
        } else if(cmd>0x17 && cmd!=0x20 && cmd!=0x30 && cmd!=0x31)return std::nullopt; // Not a code with a known size.
        if(data>words.size()-i)return std::nullopt;
        i+=data;
    }
    return out;
}

namespace {
bool normal(std::uint32_t word) {const unsigned exponent=(word>>23)&0xFF;return exponent && exponent!=0xFF;}
}

std::optional<std::vector<std::pair<std::size_t,std::size_t>>> alignSequences(std::span<const std::uint64_t> a,std::span<const std::uint64_t> b,std::size_t max_edits) {
    // The common start and end pair as they are; Myers' algorithm runs on what lies between.
    std::size_t head=0;
    while(head<a.size() && head<b.size() && a[head]==b[head])++head;
    std::size_t tail=0;
    while(tail<a.size()-head && tail<b.size()-head && a[a.size()-1-tail]==b[b.size()-1-tail])++tail;
    const auto x0=a.subspan(head,a.size()-head-tail),y0=b.subspan(head,b.size()-head-tail);
    const long n=long(x0.size()),m=long(y0.size());
    std::vector<std::pair<std::size_t,std::size_t>> pairs;
    for(std::size_t i=0;i<head;++i)pairs.emplace_back(i,i);
    if(n || m) {
        // Forward search: v[k] is the furthest x on diagonal k = x - y after d edits; each step's v is
        // kept (diagonals -d..d) to walk the script back.
        const long limit=std::min<long>(long(max_edits),n+m);
        std::vector<long> v(std::size_t(2*limit+3),0);const long mid=limit+1;
        std::vector<std::vector<long>> trace;
        long found=-1;
        for(long d=0;d<=limit && found<0;++d) {
            for(long k=-d;k<=d;k+=2) {
                long x=(k==-d || (k!=d && v[mid+k-1]<v[mid+k+1]))?v[mid+k+1]:v[mid+k-1]+1;
                long y=x-k;
                while(x<n && y<m && x0[x]==y0[y]) {++x;++y;}
                v[mid+k]=x;
                if(x>=n && y>=m) {found=d;break;}
            }
            trace.emplace_back(v.begin()+(mid-d),v.begin()+(mid+d+1));
        }
        if(found<0)return std::nullopt;
        std::vector<std::pair<std::size_t,std::size_t>> middle;
        long x=n,y=m;
        for(long d=found;d>0;--d) {
            const auto& p=trace[std::size_t(d-1)];
            const auto at=[&](long k){return p[std::size_t(k+d-1)];};
            const long k=x-y;
            const bool down=k==-d || (k!=d && at(k-1)<at(k+1));
            const long pk=down?k+1:k-1,px=at(pk),py=px-pk;
            const long sx=down?px:px+1,sy=sx-k; // After the edit, before the run of equal elements.
            while(x>sx && y>sy) {--x;--y;middle.emplace_back(std::size_t(x),std::size_t(y));}
            x=px;y=py;
        }
        while(x>0 && y>0) {--x;--y;middle.emplace_back(std::size_t(x),std::size_t(y));}
        for(auto it=middle.rbegin();it!=middle.rend();++it)pairs.emplace_back(it->first+head,it->second+head);
    }
    for(std::size_t i=0;i<tail;++i)pairs.emplace_back(a.size()-tail+i,b.size()-tail+i);
    return pairs;
}

std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                                    std::size_t* blended,std::size_t* unmatched,std::size_t max_edits) {
    const auto a=vif1Unpacks(older),b=vif1Unpacks(newer);
    if(!a || !b)return std::nullopt;
    // An unpack is known by its code and its size (the size also depends on the cycle setting before it).
    const auto keys=[](const std::vector<Vif1Unpack>& u) {
        std::vector<std::uint64_t> k;k.reserve(u.size());
        for(const auto& x:u)k.push_back(std::uint64_t(x.code)|std::uint64_t(x.words)<<32);
        return k;
    };
    const auto ka=keys(*a),kb=keys(*b);
    const auto pairs=alignSequences(ka,kb,max_edits);
    if(!pairs)return std::nullopt;
    std::vector<std::uint32_t> out(newer.begin(),newer.end());
    std::size_t changed=0;
    for(const auto& [i,j]:*pairs) {
        const auto& x=(*a)[i];const auto& y=(*b)[j];
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
    if(unmatched)*unmatched=b->size()-pairs->size();
    return out;
}
