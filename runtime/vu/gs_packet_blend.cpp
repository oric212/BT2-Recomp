#include "vu/gs_packet_blend.h"
#include "vu/vif1_blend.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <tuple>

namespace {
// A vertex write in a block: the word it starts at, whether it carries fog (XYZF2, XYZF3) and whether it is a
// 64-bit value (REGLIST, A+D) rather than a packed quadword. XYZ3 and XYZF3 (no drawing kick) count too.
struct Vertex {std::size_t at;bool fog,wide;};
struct Unit {std::uint64_t key;std::size_t first,count;bool used;};
struct Parsed {std::vector<Vertex> vertices;std::vector<Unit> units;};

std::uint64_t mix(std::uint64_t h,std::uint64_t v) {return h^(v+0x9E3779B97F4A7C15ull+(h<<6)+(h>>2));}
bool isXyz(unsigned reg) {return reg==0x04 || reg==0x05 || reg==0x0C || reg==0x0D;}
bool isFog(unsigned reg) {return reg==0x04 || reg==0x0C;}

// The vertices of one block, grouped by GIF tag. A tag whose data runs past the block ends the walk.
void parse(std::span<const std::uint32_t> w,std::size_t start,std::size_t words,Parsed& p) {
    p.vertices.clear();p.units.clear();
    std::uint64_t prim=0,tex0=0;
    std::size_t at=start;
    const std::size_t end=start+words;
    while(at+4<=end) {
        const std::uint64_t lo=w[at]|std::uint64_t(w[at+1])<<32,hi=w[at+2]|std::uint64_t(w[at+3])<<32;
        at+=4;
        const unsigned nloop=unsigned(lo&0x7FFF),flg=unsigned(lo>>58)&3,nreg=unsigned(lo>>60)?unsigned(lo>>60):16;
        if(lo>>46&1)prim=(lo>>47)&0x7FF;
        const std::size_t first=p.vertices.size();
        std::uint64_t key=mix(lo&~std::uint64_t(0x8000),hi); // Not EOP: only a block's last tag has it.
        bool keyed=false;
        const auto vertex=[&](std::size_t a,unsigned reg,bool wide) {
            if(!keyed) {key=mix(mix(key,prim),tex0);keyed=true;} // The state of the tag's first vertex.
            p.vertices.push_back({a,isFog(reg),wide});
        };
        if(flg==0) { // PACKED
            const std::size_t size=std::size_t(nloop)*nreg*4;
            if(size>end-at)return;
            for(std::size_t n=0;n<std::size_t(nloop)*nreg;++n) {
                const std::size_t q=at+n*4;
                unsigned reg=unsigned(hi>>(4*(n%nreg)))&15;
                std::uint64_t value=w[q]|std::uint64_t(w[q+1])<<32;
                bool wide=false;
                if(reg==0x0E) {reg=w[q+2]&0xFF;wide=true;} // A+D: the register in the high half.
                if(reg==0x00)prim=wide?value&0x7FF:w[q]&0x7FF;
                else if(reg==0x06 || reg==0x07)tex0=value;
                else if(isXyz(reg))vertex(q,reg,wide);
            }
            at+=size;
        } else if(flg==1) { // REGLIST: 64-bit values
            const std::size_t count=std::size_t(nloop)*nreg,size=(count*2+3)/4*4;
            if(size>end-at)return;
            for(std::size_t n=0;n<count;++n) {
                const std::size_t q=at+n*2;
                const unsigned reg=unsigned(hi>>(4*(n%nreg)))&15;
                if(reg==0x00)prim=w[q]&0x7FF;
                else if(reg==0x06 || reg==0x07)tex0=w[q]|std::uint64_t(w[q+1])<<32;
                else if(isXyz(reg))vertex(q,reg,true);
            }
            at+=size;
        } else { // IMAGE
            const std::size_t size=std::size_t(nloop)*4;
            if(size>end-at)return;
            at+=size;
        }
        if(p.vertices.size()>first)p.units.push_back({key,first,p.vertices.size()-first,false});
    }
}

struct Position {std::int64_t x,y,z;};
Position read(std::span<const std::uint32_t> w,const Vertex& v) {
    if(v.wide)return {w[v.at]&0xFFFF,w[v.at]>>16,v.fog?w[v.at+1]&0xFFFFFF:w[v.at+1]};
    return {w[v.at]&0xFFFF,w[v.at+1]&0xFFFF,v.fog?(w[v.at+2]>>4)&0xFFFFFF:w[v.at+2]};
}
void write(std::vector<std::uint32_t>& out,const Vertex& v,const Position& p) {
    const auto x=std::uint32_t(p.x)&0xFFFF,y=std::uint32_t(p.y)&0xFFFF;
    if(v.wide) {
        out[v.at]=x|y<<16;
        out[v.at+1]=v.fog?(out[v.at+1]&0xFF000000u)|(std::uint32_t(p.z)&0xFFFFFF):std::uint32_t(p.z);
    } else {
        out[v.at]=(out[v.at]&0xFFFF0000u)|x;
        out[v.at+1]=(out[v.at+1]&0xFFFF0000u)|y;
        out[v.at+2]=v.fog?(out[v.at+2]&0xF000000Fu)|(std::uint32_t(p.z)&0xFFFFFF)<<4:std::uint32_t(p.z);
    }
}
std::int64_t between(std::int64_t older,std::int64_t newer,float t) {return newer-std::llround(double(1-t)*double(newer-older));}

// A block's identity: its opening GIF tag without its loop count, and when that tag writes A+D, the first
// register it writes; not its length or data.
std::vector<std::uint64_t> keys(std::span<const std::uint32_t> w,const std::vector<Vif1Unpack>& blocks) {
    std::vector<std::uint64_t> k;k.reserve(blocks.size());
    for(const auto& b:blocks) {
        std::uint64_t h=0;
        for(std::size_t n=0;n<std::min<std::size_t>(b.words,4);++n)h=mix(h,n==0?w[b.start]&~0x7FFFu:w[b.start+n]);
        if(b.words>=8 && (w[b.start+1]>>26&3)==0 && (w[b.start+2]&15)==0x0E)h=mix(h,w[b.start+6]&0xFF);
        k.push_back(h);
    }
    return k;
}
}

std::size_t blendGsPackets(std::vector<std::uint32_t>& out,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                           const std::vector<Vif1Unpack>* older_blocks,const std::vector<Vif1Unpack>* newer_blocks,float max_pixels) {
    if(t>=1)return 0;
    std::optional<Vif1Walk> walked_older,walked_newer;
    if(!older_blocks && (walked_older=walkVif1(older)))older_blocks=&walked_older->directs;
    if(!newer_blocks && (walked_newer=walkVif1(newer)))newer_blocks=&walked_newer->directs;
    const auto* a=older_blocks;const auto* b=newer_blocks;
    if(!a || !b || a->empty() || b->empty())return 0;
    const auto pairs=alignSequences(keys(older,*a),keys(newer,*b),1024);
    if(!pairs)return 0;
    const std::int64_t limit=std::int64_t(max_pixels*16);
    // Every unit of the newer list's changed or unpaired blocks: its vertices, and where a partner was found, the
    // partner's positions (from), in the same order. Kept per thread from picture to picture.
    struct Moving {std::size_t first,count;bool paired;};
    struct Scratch {
        Parsed pa,pb;
        std::vector<std::tuple<std::uint64_t,std::int64_t,std::size_t>> keyed; // (key, first vertex's x, older unit)
        std::vector<std::pair<std::uint64_t,std::size_t>> corners;             // (position, newer unit)
        std::vector<Moving> units;std::vector<Vertex> vertices;std::vector<Position> from;
        std::vector<std::size_t> group;std::vector<char> whole,has_partner;
    };
    thread_local Scratch s;
    auto& [pa,pb,keyed,corners,units,vertices,from,group,whole,has_partner]=s;
    units.clear();vertices.clear();from.clear();
    const auto add=[&](const Unit& unit,const Unit* partner) {
        units.push_back({vertices.size(),unit.count,partner!=nullptr});
        for(std::size_t n=0;n<unit.count;++n) {
            vertices.push_back(pb.vertices[unit.first+n]);
            from.push_back(partner?read(older,pa.vertices[partner->first+n]):Position{});
        }
    };
    has_partner.assign(b->size(),0);
    for(const auto& [i,j]:*pairs) {
        has_partner[j]=1;
        const auto& x=(*a)[i];const auto& y=(*b)[j];
        if(x.words==y.words && std::memcmp(older.data()+x.start,newer.data()+y.start,x.words*4)==0)continue;
        parse(older,x.start,x.words,pa);parse(newer,y.start,y.words,pb);
        keyed.clear();
        for(std::size_t u=0;u<pa.units.size();++u)keyed.emplace_back(pa.units[u].key,read(older,pa.vertices[pa.units[u].first]).x,u);
        std::sort(keyed.begin(),keyed.end());
        for(const auto& unit:pb.units) {
            std::size_t best=SIZE_MAX;std::int64_t best_distance=0;
            // Only candidates of its kind whose first vertex is within the limit across (sorted that way).
            const std::int64_t x=read(newer,pb.vertices[unit.first]).x;
            for(auto c=std::lower_bound(keyed.begin(),keyed.end(),std::tuple<std::uint64_t,std::int64_t,std::size_t>(unit.key,x-limit,0));
                c!=keyed.end() && std::get<0>(*c)==unit.key && std::get<1>(*c)<=x+limit;++c) {
                const std::size_t index=std::get<2>(*c);
                const auto& candidate=pa.units[index];
                if(candidate.used || candidate.count!=unit.count)continue;
                // Every vertex within the limit, and all moving alike (a quarter of the limit around the first move):
                // a polygon the game clips anew each frame (the sky) has its corners on other edges. A coordinate
                // the same in both frames is held by the edge the game clips to (the destroyed Namek's backdrop:
                // triangles cut at its guard band, x 541.69 and -29.75, y -26, the other corners moving 34 pixels)
                // and is not compared: counted, it left the whole backdrop at the newer frame's place, and the
                // lava behind it showed through where neither frame has it (2026-11-11).
                std::int64_t distance=0,dx0=0,dy0=0;
                bool alike=true,moved_x=false,moved_y=false;
                for(std::size_t n=0;n<unit.count && alike;++n) {
                    const Position p=read(older,pa.vertices[candidate.first+n]),q=read(newer,pb.vertices[unit.first+n]);
                    const std::int64_t dx=q.x-p.x,dy=q.y-p.y;
                    if(dx && !moved_x) {dx0=dx;moved_x=true;}
                    if(dy && !moved_y) {dy0=dy;moved_y=true;}
                    distance=std::max({distance,std::abs(dx),std::abs(dy)});
                    alike=distance<=limit && (!dx || std::abs(dx-dx0)<=limit/4) && (!dy || std::abs(dy-dy0)<=limit/4);
                }
                if(alike && (best==SIZE_MAX || distance<best_distance)) {best=index;best_distance=distance;}
            }
            if(best!=SIZE_MAX)pa.units[best].used=true;
            add(unit,best!=SIZE_MAX?&pa.units[best]:nullptr);
        }
    }
    for(std::size_t j=0;j<b->size();++j)if(!has_partner[j]) {
        parse(newer,(*b)[j].start,(*b)[j].words,pb);
        for(const auto& unit:pb.units)add(unit,nullptr);
    }
    // Units that share a vertex position anywhere in the list (a mesh the game tiles, as the sky's polygons, each
    // in a block of its own) move together or not at all: one blended beside one left as it is opens a crack.
    group.resize(units.size());
    for(std::size_t u=0;u<units.size();++u)group[u]=u;
    const auto root=[&](std::size_t u){while(group[u]!=u)u=group[u]=group[group[u]];return u;};
    corners.clear();
    for(std::size_t u=0;u<units.size();++u)for(std::size_t n=0;n<units[u].count;++n) {
        const Position q=read(newer,vertices[units[u].first+n]);
        corners.emplace_back(std::uint64_t(q.x)|std::uint64_t(q.y)<<16,u);
    }
    std::sort(corners.begin(),corners.end());
    for(std::size_t n=1;n<corners.size();++n)if(corners[n].first==corners[n-1].first)group[root(corners[n].second)]=root(corners[n-1].second);
    whole.assign(units.size(),1);
    for(std::size_t u=0;u<units.size();++u)if(!units[u].paired)whole[root(u)]=0;
    std::size_t blended=0;
    for(std::size_t u=0;u<units.size();++u) {
        if(!units[u].paired || !whole[root(u)])continue;
        for(std::size_t n=units[u].first;n<units[u].first+units[u].count;++n) {
            const Position p=from[n],q=read(newer,vertices[n]);
            write(out,vertices[n],{between(p.x,q.x,t),between(p.y,q.y,t),between(p.z,q.z,t)});
        }
        blended+=units[u].count;
    }
    return blended;
}
