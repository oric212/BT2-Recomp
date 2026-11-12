// In-between VIF1 lists (runtime/vu/vif1_blend.h) on synthetic lists (no game data).
#include "vu/vif1_blend.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"VIF1 blend check failed: "<<what<<'\n';std::exit(1);}}
std::uint32_t bits(float f) {std::uint32_t w;std::memcpy(&w,&f,4);return w;}
float value(std::uint32_t w) {float f;std::memcpy(&f,&w,4);return f;}
// STCYCL 1,1; UNPACK V4-32 of two quadwords; MSCAL; DIRECT of one quadword; UNPACK V2-16 of one element.
std::vector<std::uint32_t> list(float a,float b,std::uint32_t tag,std::uint32_t direct) {
    return {0x01000101u,0x6C020000u,bits(a),tag,bits(1.0f),bits(0.0f),bits(b),bits(2.0f),bits(3.0f),bits(4.0f),
            0x14000000u,0x50000001u,direct,0,0,0,0x65010010u,0x00020001u};
}

// The GS packets the game builds itself (vu/gs_packet_blend.h). A flame as an aura draws it: a REGLIST tag of
// PRIM, TEX0 and four RGBAQ, ST, XYZF2 (8 quadwords), its corners 8 pixels apart from (x, y) at depth z.
struct Flame {std::uint32_t tex;unsigned x,y;std::uint32_t z,q;unsigned stretch=0;}; // stretch: the last corner further right
void flame(std::vector<std::uint32_t>& l,const Flame& f) {
    l.insert(l.end(),{1u,0xE4000000u,0x42142160u,0x00421421u}); // NLOOP 1, REGLIST, 14 registers
    l.insert(l.end(),{0x5Cu,0u,f.tex,0x00000004u});              // PRIM, TEX0
    for(unsigned c=0;c<4;++c) {
        const std::uint32_t x=(f.x+(c&1)*8+(c==3?f.stretch:0))*16,y=(f.y+(c>>1)*8)*16;
        l.insert(l.end(),{0x80808080u,f.q,c,c,x|y<<16,f.z|0x7Fu<<24}); // RGBAQ, ST, XYZF2 (fog 0x7F)
    }
}
// Two DIRECT blocks: one that opens with an A+D of ALPHA, then the flames; one of a single A+D XYZ2.
std::vector<std::uint32_t> flames(const std::vector<Flame>& fs,std::uint32_t xy) {
    std::vector<std::uint32_t> data{0x8001u,0x10000000u,0x0Eu,0u,0x48u,0u,0x42u,0u};
    for(const auto& f:fs)flame(data,f);
    if(!fs.empty())data[8+(fs.size()-1)*32]|=0x8000u; // EOP on the last tag, as the game ends a packet
    std::vector<std::uint32_t> l{0x50000000u|std::uint32_t(data.size()/4)};
    l.insert(l.end(),data.begin(),data.end());
    l.insert(l.end(),{0x50000002u,0x8001u,0x10000000u,0x0Eu,0u,xy,0x1234u,0x05u,0u});
    return l;
}
// Word k (0 to 5: RGBAQ, ST, XYZF2) of corner c of flame f.
std::uint32_t word(const std::vector<std::uint32_t>& l,std::size_t f,unsigned c,unsigned k) {return l[1+8+f*32+8+c*6+k];}
void gsPacketTests() {
    // The newer frame has the flames in another order, one more, one far away, and another Q.
    const std::vector<Flame> older{{0x2B10,100,100,30000,1},{0x2B00,200,100,31000,1},{0x2B10,300,300,32000,1},{0x2B20,400,200,30000,1},{0x2B30,500,100,30000,1},{0x2B30,508,100,30000,1}};
    const std::vector<Flame> newer{{0x2B00,210,120,33000,2},{0x2B10,120,110,34000,2},{0x2B70,50,50,35000,2},{0x2B10,900,300,36000,2},{0x2B20,401,200,30000,2,60},{0x2B30,502,100,30000,2},{0x2B30,510,100,30000,2,60}};
    const auto a=flames(older,0x00400040u),b=flames(newer,0x00800080u);
    std::size_t blended=0;
    const auto mid=blendVif1(a,b,0.5f,&blended);
    check(mid && mid->size()==b.size(),"lists with GS packets blend");
    check((word(*mid,0,0,4)&0xFFFF)==205*16 && (word(*mid,0,0,4)>>16)==110*16,"a flame moves halfway to its partner of its kind");
    check((word(*mid,0,3,5)&0xFFFFFF)==32000 && (word(*mid,0,3,5)>>24)==0x7F,"its depth blended, its fog kept");
    check((word(*mid,1,0,4)&0xFFFF)==110*16 && (word(*mid,1,0,5)&0xFFFFFF)==32000,"a flame takes the nearest of its kind");
    check(word(*mid,0,0,1)==2 && word(*mid,0,0,2)==0,"colours and texture coordinates stay the newer frame's");
    for(unsigned c=0;c<4;++c)check(word(*mid,2,c,4)==word(b,2,c,4) && word(*mid,3,c,4)==word(b,3,c,4),"a flame without a partner, or too far, is the newer one");
    for(unsigned c=0;c<4;++c)check(word(*mid,4,c,4)==word(b,4,c,4),"a polygon whose corners moved unlike each other (clipped anew) is the newer one");
    for(unsigned c=0;c<4;++c)check(word(*mid,5,c,4)==word(b,5,c,4),"a polygon sharing corners with one left as it is stays too (no crack)");
    check((*mid)[b.size()-4]==0x00600060u && (*mid)[b.size()-3]==0x1234u,"an A+D vertex blended");
    check(blended==3*(4+4+1),"blended vertices counted");
    const auto end=blendVif1(a,b,1.0f);
    check(end && *end==b,"GS packets exact at t = 1");
    // The same across blocks (each sky polygon comes in a block of its own): the second block's square, clipped anew,
    // keeps the first block's square, which shares its corners, where it is.
    auto two=[](unsigned second_x,unsigned stretch) {
        auto l=flames({{0x2B40,600,100,30000,1}},0x00400040u);
        const auto r=flames({{0x2B40,second_x,100,30000,1,stretch}},0x00400040u);
        l.insert(l.end(),r.begin(),r.end());
        return l;
    };
    const auto c=two(608,0),d=two(610,60);
    auto shifted=d;for(unsigned k=0;k<4;++k)shifted[1+8+8+k*6+4]=d[1+8+8+k*6+4]+(2u*16); // first square 2 pixels right
    const auto across=blendVif1(c,shifted,0.5f);
    check(across && *across==shifted,"squares sharing corners across blocks stay together");
    // The older block's last flame (with EOP) pairs with a newer one that is no longer last.
    const auto last=flames({{0x2B50,700,100,30000,1}},0x00400040u),more=flames({{0x2B50,702,100,30000,1},{0x2B60,800,100,30000,1}},0x00400040u);
    const auto eop=blendVif1(last,more,0.5f);
    check(eop && (word(*eop,0,0,4)&0xFFFF)==701*16,"the end of packet mark is no part of a flame's kind");
    // A polygon cut at the edge the game clips to (the destroyed Namek's backdrop): its right corners stay on that edge
    // (x 608 in both frames) while the others move 40 pixels; it moves halfway, its corners on the edge staying there.
    const auto cut_older=flames({{0x2B90,600,150,30000,1}},0x00400040u);
    auto cut_newer=flames({{0x2B90,640,150,30000,1}},0x00400040u);
    for(unsigned c:{1u,3u})cut_newer[1+8+8+c*6+4]=(cut_newer[1+8+8+c*6+4]&0xFFFF0000u)|608u*16;
    const auto cut=blendVif1(cut_older,cut_newer,0.5f);
    check(cut && (word(*cut,0,0,4)&0xFFFF)==620*16 && (word(*cut,0,2,4)&0xFFFF)==620*16,"a polygon cut at the clip edge moves halfway");
    check((word(*cut,0,1,4)&0xFFFF)==608*16 && (word(*cut,0,3,4)&0xFFFF)==608*16,"its corners on the edge stay there");
}
}

int main() {
    const auto older=list(1.0f,10.0f,0x00000005u,0x1111u),newer=list(3.0f,20.0f,0x00000007u,0x2222u);
    const auto unpacks=vif1Unpacks(newer);
    check(unpacks && unpacks->size()==2 && (*unpacks)[0].start==2 && (*unpacks)[0].words==8 && (*unpacks)[1].words==1,"unpacks found and sized");
    std::size_t blended=0;
    const auto mid=blendVif1(older,newer,0.5f,&blended);
    // Only matrices are blended (a bone or camera block): this small unpack is the newer frame's.
    check(mid.has_value() && blended==0 && value((*mid)[2])==3.0f && value((*mid)[6])==20.0f,"data outside matrices not blended");
    check((*mid)[3]==0x00000007u,"a word that is not a normal float is the newer one");
    check((*mid)[12]==0x2222u,"DIRECT data is the newer frame's");
    check((*mid)[4]==bits(1.0f) && (*mid)[17]==0x00020001u,"equal words and other formats unchanged");
    // Lists that differ in places: unpacks are aligned by their codes; the rest pairs and is blended.
    auto shorter=newer;shorter.resize(16);
    std::size_t unmatched=9;
    const auto without_last=blendVif1(older,shorter,0.5f,&blended,&unmatched);
    check(without_last && blended==0 && unmatched==0 && value((*without_last)[2])==3.0f,"a missing unpack leaves the others paired");
    auto resized=newer;resized[1]=0x6C010000u;resized.erase(resized.begin()+6,resized.begin()+10);
    const auto other_size=blendVif1(older,resized,0.5f,&blended,&unmatched);
    check(other_size && blended==0 && unmatched==1 && value((*other_size)[2])==3.0f,"an unpack of another code is the newer one");
    // An unpack inserted in front: the ones after it still pair.
    std::vector<std::uint32_t> inserted{0x6C010040u,bits(9.0f),bits(9.0f),bits(9.0f),bits(9.0f)};
    inserted.insert(inserted.end(),newer.begin(),newer.end());
    const auto with_insert=blendVif1(older,inserted,0.5f,&blended,&unmatched);
    check(with_insert && blended==0 && unmatched==1 && value((*with_insert)[1])==9.0f && value((*with_insert)[7])==3.0f,"an inserted unpack keeps the rest paired");
    // Alignment itself: a shortest edit script's equal elements.
    const std::vector<std::uint64_t> sa{1,2,3,4,5,6},sb{1,3,4,7,5,6,8};
    const auto aligned=alignSequences(sa,sb,16);
    check(aligned && aligned->size()==5 && (*aligned)[1]==std::pair<std::size_t,std::size_t>(2,1) && (*aligned)[3]==std::pair<std::size_t,std::size_t>(4,4),"sequences aligned");
    check(!alignSequences(sa,std::vector<std::uint64_t>{9,9,9,9,9,9},3),"too many edits refused");
    // STCYCL with WL > CL: the data covers CL of every WL elements.
    const std::vector<std::uint32_t> skipping{0x01000201u,0x6C040000u,1,2,3,4,5,6,7,8};
    const auto u=vif1Unpacks(skipping);
    check(u && u->size()==1 && (*u)[0].words==8,"WL > CL sizes the data by CL");
    check(!vif1Unpacks(std::vector<std::uint32_t>{0x6C020000u,1,2}),"data past the end is refused");
    // A block of `quadwords` constants (2.0) with a rigid transform at quadword `at`: turned by `degrees`
    // about z, at (x, 0, 0); the same block shifted by `shift` quadwords is other data in the same chunk.
    auto block=[&](unsigned quadwords,unsigned at,float degrees,float x,unsigned shift=0) {
        std::vector<std::uint32_t> l{0x01000101u,0x6C000000u|(quadwords<<16)};
        const float c=std::cos(degrees*3.14159265f/180),s=std::sin(degrees*3.14159265f/180);
        const float m[4][4]={{c,s,0,0},{-s,c,0,0},{0,0,1,0},{x,0,0,1}};
        for(unsigned q=0;q<quadwords;++q)for(unsigned k=0;k<4;++k) {
            const unsigned r=(q+shift)%quadwords;
            l.push_back(bits(r>=at && r<at+4?m[r-at][k]:2.0f+float(r)));
        }
        return l;
    };
    auto near=[](float a,float b){return std::abs(a-b)<1e-5f*std::max(1.0f,std::abs(b));};
    const std::size_t m0=2+5*4; // The matrix's first word (quadword 5).
    // A transform turning by 40 degrees: halfway it has turned by 20 with its axes' length kept (not shrunk
    // to cos 20); the position moves halfway. It is found wherever it stands in the block.
    std::size_t refused=9;
    const auto turned=blendVif1(block(20,5,0,10),block(20,5,40,20),0.5f,&blended,nullptr,1024,&refused);
    check(turned && refused==0 && blended==12,"a rigid transform blended");
    check(near(std::hypot(value((*turned)[m0]),value((*turned)[m0+1])),1.0f) && near(std::atan2(value((*turned)[m0+1]),value((*turned)[m0]))*180/3.14159265f,20.0f),"an axis turns halfway and keeps its length");
    check(near(value((*turned)[m0+12]),15.0f) && value((*turned)[m0+15])==1.0f && value((*turned)[2])==2.0f,"the position moves halfway, the rest is the newer frame's");
    // A fast turn (90 degrees in a frame) is blended halfway; a turn of more than 150 degrees or a jump of the
    // position is not (another bone, a cut).
    const auto fast=blendVif1(block(20,5,0,10),block(20,5,90,10),0.5f,&blended);
    check(fast && blended==12 && near(std::atan2(value((*fast)[m0+1]),value((*fast)[m0]))*180/3.14159265f,45.0f)
          && near(std::hypot(value((*fast)[m0]),value((*fast)[m0+1])),1.0f),"a fast turn blended halfway");
    const auto spun=blendVif1(block(20,5,0,10),block(20,5,160,10),0.5f,&blended);
    check(spun && blended==0 && *spun==block(20,5,160,10),"a turn past 150 degrees stays the newer frame's");
    const auto jumped=blendVif1(block(20,5,0,10),block(20,5,0,900),0.5f,&blended);
    check(jumped && blended==0 && *jumped==block(20,5,0,900),"a jump stays the newer frame's");
    check(sameScene(0,13000) && sameScene(130,13000) && sameScene(650,13000),"a fight's changed pairs keep the scene");
    check(!sameScene(651,13000) && !sameScene(1400,13000),"a fighter drawn another way is another scene");
    // Data shifted in its chunk (here by 3 quadwords) is not searched for: it stays the newer frame's (a pairing
    // found by a shift could take another bone).
    const auto shifted=blendVif1(block(20,5,0,10),block(20,5,10,12,3),0.5f,&blended,nullptr,1024,&refused);
    check(shifted && blended==0 && *shifted==block(20,5,10,12,3),"shifted data stays the newer frame's");
    // Other data around a transform at the same place: the transform is blended, the rest is the newer frame's.
    auto unrelated=block(20,5,10,12);for(std::size_t w=2;w<unrelated.size();++w)if((w-2)/4<5 || (w-2)/4>8)unrelated[w]=bits(-7.0f-float(w));
    const auto other=blendVif1(block(20,5,0,10),unrelated,0.5f,&blended,nullptr,1024,&refused);
    check(other && blended==12 && near(value((*other)[m0+12]),11.0f) && (*other)[2]==unrelated[2],"a transform in other data blended alone");
    // A camera block (12 quadwords, the viewport in the last four the same): its first eight blended linearly.
    const auto camera=blendVif1(block(12,0,0,100),block(12,0,0,300),0.5f,&blended);
    check(camera && value((*camera)[2+12])==200.0f && value((*camera)[2+8*4])==2.0f+8,"a camera block blended linearly");
    // A copy of the camera's screen matrix elsewhere (an object's data) becomes the blended one too.
    auto with_copy=[&](float x) {
        auto l=block(12,0,0,x);const auto copy=std::vector<std::uint32_t>(l.begin()+2,l.begin()+2+16);
        l.push_back(0x6C060000u);for(unsigned q=0;q<6;++q)for(unsigned k=0;k<4;++k)l.push_back(q>=1 && q<5?copy[(q-1)*4+k]:bits(5.0f));
        return l;
    };
    const auto copied=blendVif1(with_copy(100),with_copy(300),0.5f,&blended);
    check(copied && value((*copied)[2+48+1+4+12])==200.0f && value((*copied)[2+48+1])==5.0f,"a camera's copy follows the blended camera");
    // Two cameras (words (base + w) x scale in their first eight quadwords) and an unpack holding a copy of the
    // first camera's second half, then of the second camera's first half: each copy follows its own camera.
    auto two_cameras=[&](float scale) {
        std::vector<std::uint32_t> l{0x01000101u};
        std::vector<std::uint32_t> halves[2][2];
        for(unsigned c=0;c<2;++c) {
            l.push_back(0x6C0C0000u);
            for(unsigned w=0;w<48;++w) {
                const std::uint32_t v=bits(w<32?(100.0f+200.0f*float(c)+float(w))*scale*float(c+1):2.0f);
                l.push_back(v);if(w<32)halves[c][w/16].push_back(v);
            }
        }
        l.push_back(0x6C0A0000u);
        for(unsigned w=0;w<4;++w)l.push_back(bits(5.0f));
        l.insert(l.end(),halves[0][1].begin(),halves[0][1].end());
        l.insert(l.end(),halves[1][0].begin(),halves[1][0].end());
        for(unsigned w=0;w<4;++w)l.push_back(bits(5.0f));
        return l;
    };
    const auto both=blendVif1(two_cameras(1.0f),two_cameras(3.0f),0.5f,&blended);
    const std::size_t copies_at=1+2*49+1+4;
    check(both && value((*both)[copies_at])==(100.0f+16)*2 && value((*both)[copies_at+16])==(300.0f)*4 && value((*both)[copies_at+32])==5.0f
          && value((*both)[copies_at-1])==5.0f && blended==2*32+2*16,"copies of two cameras' halves follow their cameras");
    // At t = 1 the result is the newer list exactly.
    const auto end=blendVif1(block(20,5,0,10),block(20,5,40,20),1.0f);
    check(end && *end==block(20,5,40,20),"exact at t = 1");
    // A camera block (12 quadwords) whose view axis turns by 90 degrees is a cut; a small turn is not.
    // (Its view axes are quadwords 4 to 6: a transform there turned by 90 degrees about z, or by 5.)
    check(cameraTurn(block(12,4,0,0),block(12,4,90,0))<0.01f && cameraTurn(block(12,4,0,0),block(12,4,5,0))>0.99f,"camera cuts found");
    gsPacketTests();
    std::cout<<"VIF1 blend tests passed\n";
    return 0;
}
