#include "vu/vif1_blend.h"
#include <algorithm>
#include <array>
#include <cmath>
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

namespace {
float wordFloat(std::uint32_t w) {float f;std::memcpy(&f,&w,4);return f;}
std::uint32_t floatWord(float f) {std::uint32_t w;std::memcpy(&w,&f,4);return w;}
// Four words equal (a quadword), compared in place.
bool sameQuad(const std::uint32_t* a,const std::uint32_t* b) {return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];}
// A rigid transform at word `at`: three axes (w 0) of sensible length at nearly right angles, then a
// position (w 1). BT2's bone matrices look like this wherever they stand in a block; shifted data,
// constants and the screen matrices do not.
bool rigid(std::span<const std::uint32_t> d,std::size_t at) {
    const auto v=[&](unsigned q,unsigned k){return wordFloat(d[at+q*4+k]);};
    if(std::abs(v(0,3))>1e-6f || std::abs(v(1,3))>1e-6f || std::abs(v(2,3))>1e-6f || v(3,3)!=1.0f)return false;
    float length[3];
    for(unsigned q=0;q<3;++q) {
        float s=0;for(unsigned k=0;k<3;++k)s+=v(q,k)*v(q,k);
        length[q]=std::sqrt(s);
        if(!(length[q]>0.05f && length[q]<20.0f))return false;
    }
    for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b) {
        float dot=0;for(unsigned k=0;k<3;++k)dot+=v(a,k)*v(b,k);
        if(std::abs(dot)>0.2f*length[a]*length[b])return false;
    }
    for(unsigned k=0;k<3;++k)if(!std::isfinite(v(3,k)))return false;
    return true;
}
// The same transform in both frames, blended at t into `to` (its 16 words): each axis turns (its length
// blended), the position moves; false when an axis turned further than `min_cos` allows or the position moved
// further than `max_move` (another bone, a cut).
bool blendRigid(std::span<const std::uint32_t> older,std::size_t a,std::span<const std::uint32_t> newer,std::size_t b,float t,
                float min_cos,float max_move,std::uint32_t* to) {
    float axes[3][3];
    for(unsigned q=0;q<3;++q) {
        float p[3],r[3],lp=0,lr=0,dot=0;
        for(unsigned k=0;k<3;++k) {p[k]=wordFloat(older[a+q*4+k]);r[k]=wordFloat(newer[b+q*4+k]);lp+=p[k]*p[k];lr+=r[k]*r[k];dot+=p[k]*r[k];}
        lp=std::sqrt(lp);lr=std::sqrt(lr);
        if(dot<min_cos*lp*lr)return false;
        float m[3],lm=0;
        for(unsigned k=0;k<3;++k) {m[k]=p[k]+t*(r[k]-p[k]);lm+=m[k]*m[k];}
        lm=std::sqrt(lm);
        const float scale=(lp+t*(lr-lp))/lm;
        for(unsigned k=0;k<3;++k)axes[q][k]=m[k]*scale;
    }
    float moved=0;
    for(unsigned k=0;k<3;++k) {const float d=wordFloat(newer[b+12+k])-wordFloat(older[a+12+k]);moved+=d*d;}
    if(!(moved<=max_move*max_move))return false;
    std::copy(newer.begin()+std::ptrdiff_t(b),newer.begin()+std::ptrdiff_t(b+16),to);
    for(unsigned q=0;q<3;++q)for(unsigned k=0;k<3;++k)to[q*4+k]=floatWord(axes[q][k]);
    for(unsigned k=0;k<3;++k) {const float p=wordFloat(older[a+12+k]),r=wordFloat(newer[b+12+k]);to[12+k]=floatWord(r-(1-t)*(r-p));}
    return true;
}
// A camera block (12 quadwords): its last four (the viewport's scale and offset) are the same in both.
bool sameCamera(std::span<const std::uint32_t> older,std::size_t a,std::span<const std::uint32_t> newer,std::size_t b) {
    return std::equal(older.begin()+std::ptrdiff_t(a+32),older.begin()+std::ptrdiff_t(a+48),newer.begin()+std::ptrdiff_t(b+32));
}
}

// The unpacks of two lists aligned by their codes and sizes (alignSequences).
std::optional<Vif1Pairing> pairVif1(const std::vector<Vif1Unpack>& older,const std::vector<Vif1Unpack>& newer,std::size_t max_edits) {
    const auto keys=[](const std::vector<Vif1Unpack>& u) {
        std::vector<std::uint64_t> k;k.reserve(u.size());
        for(const auto& x:u)k.push_back(std::uint64_t(x.code)|std::uint64_t(x.words)<<32);
        return k;
    };
    auto pairs=alignSequences(keys(older),keys(newer),max_edits);
    if(!pairs)return std::nullopt;
    return Vif1Pairing{&older,&newer,std::move(*pairs)};
}

std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                                    std::size_t* blended,std::size_t* unmatched,std::size_t max_edits,std::size_t* refused) {
    const auto a=vif1Unpacks(older),b=vif1Unpacks(newer);
    if(!a || !b)return std::nullopt;
    const auto pairing=pairVif1(*a,*b,max_edits);
    if(!pairing)return std::nullopt;
    return blendVif1(*pairing,older,newer,t,blended,unmatched,refused);
}

std::vector<std::uint32_t> blendVif1(const Vif1Pairing& pairing,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                     std::size_t* blended,std::size_t* unmatched,std::size_t* refused) {
    const auto* a=pairing.older;const auto* b=pairing.newer;const auto* pairs=&pairing.pairs;
    std::vector<std::uint32_t> out(newer.begin(),newer.end());
    if(t>=1)return out; // The newer list as it is.
    std::size_t changed=0,others=0;
    // Each camera's screen matrices (its first eight quadwords, as the newer frame has them) and their blend.
    struct Camera {std::array<std::uint32_t,32> newer,blend;};
    std::vector<Camera> cameras;
    for(const auto& [i,j]:*pairs) {
        const auto& x=(*a)[i];const auto& y=(*b)[j];
        if(y.format!=0xC)continue; // V4-32
        const std::size_t quadwords=y.words/4;
        if(std::memcmp(older.data()+x.start,newer.data()+y.start,x.words*4)==0)continue;
        // A camera block: its screen matrices (projective, not rigid) blended linearly, and noted, so that every
        // copy of them in the list (objects carry the camera's matrices with their own data) moves with it.
        if(quadwords==12 && sameCamera(older,x.start,newer,y.start)) {
            Camera c;
            for(std::size_t w=0;w<32;++w) {
                const std::uint32_t p=older[x.start+w],q=newer[y.start+w];
                c.newer[w]=q;c.blend[w]=q;
                if(p==q || !normal(p) || !normal(q))continue;
                const float fp=wordFloat(p),fq=wordFloat(q);
                c.blend[w]=floatWord(fq-(1-t)*(fq-fp));
            }
            cameras.push_back(c);
            continue;
        }
        // Rigid transforms (bones) where the older frame has one at the same place (the pair's unpacks have the
        // same code and size). A bone may turn by up to 150 degrees in a frame (fast moves; one left at the newer
        // pose beside blended ones tears the skin between them). Data that moved within its chunk (something
        // before it changed size) is not searched for: a pairing found by a shift could take another bone and put
        // a piece of a fighter elsewhere (measured on in-between pictures, 2026-10-26); it stays the newer frame's.
        std::size_t done=0;
        for(std::size_t q=0;q+4<=quadwords;) {
            if(!rigid(newer,y.start+q*4)) {++q;continue;}
            std::array<std::uint32_t,16> to;
            if(rigid(older,x.start+q*4) && blendRigid(older,x.start+q*4,newer,y.start+q*4,t,-0.866f,200.0f,to.data())) {
                std::copy(to.begin(),to.end(),out.begin()+std::ptrdiff_t(y.start+q*4));changed+=12;++done;
            }
            q+=4;
        }
        if(!done)++others;
    }
    // Every copy of a camera's screen matrices in the newer list's V4-32 data: the blended camera.
    for(const auto& c:cameras)for(const auto& y:*b) {
        if(y.format!=0xC)continue;
        for(std::size_t half=0;half<2;++half)
            for(std::size_t q=0;q+4<=y.words/4;++q) {
                const std::size_t at=y.start+q*4;
                const auto* m=c.newer.data()+half*16;const auto* w=newer.data()+at;
                if(w[0]!=m[0] || !sameQuad(w,m) || !sameQuad(w+4,m+4) || !sameQuad(w+8,m+8) || !sameQuad(w+12,m+12))continue;
                std::copy(c.blend.begin()+std::ptrdiff_t(half*16),c.blend.begin()+std::ptrdiff_t(half*16+16),out.begin()+std::ptrdiff_t(at));
                changed+=16;q+=3;
            }
    }
    if(blended)*blended=changed;
    if(unmatched)*unmatched=b->size()-pairs->size();
    if(refused)*refused=others;
    return out;
}

float cameraTurn(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,std::size_t max_edits) {
    const auto a=vif1Unpacks(older),b=vif1Unpacks(newer);
    if(!a || !b)return 1;
    const auto pairing=pairVif1(*a,*b,max_edits);
    return pairing?cameraTurn(*pairing,older,newer):1;
}

float cameraTurn(const Vif1Pairing& pairing,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer) {
    float smallest=1;
    for(const auto& [i,j]:pairing.pairs) {
        const auto& x=(*pairing.older)[i];const auto& y=(*pairing.newer)[j];
        if(y.format!=0xC || y.words!=12*4 || !sameCamera(older,x.start,newer,y.start))continue;
        for(unsigned q=4;q<7;++q) {
            float dot=0,la=0,lb=0;
            for(int k=0;k<3;++k) {
                const float p=wordFloat(older[x.start+q*4+k]),r=wordFloat(newer[y.start+q*4+k]);
                dot+=p*r;la+=p*p;lb+=r*r;
            }
            if(la>1e-12f && lb>1e-12f && std::isfinite(dot))smallest=std::min(smallest,dot/std::sqrt(la*lb));
        }
    }
    return smallest;
}
