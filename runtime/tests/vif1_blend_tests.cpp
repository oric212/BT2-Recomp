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
    // At t = 1 the result is the newer list exactly.
    const auto end=blendVif1(block(20,5,0,10),block(20,5,40,20),1.0f);
    check(end && *end==block(20,5,40,20),"exact at t = 1");
    // A camera block (12 quadwords) whose view axis turns by 90 degrees is a cut; a small turn is not.
    // (Its view axes are quadwords 4 to 6: a transform there turned by 90 degrees about z, or by 5.)
    check(cameraTurn(block(12,4,0,0),block(12,4,90,0))<0.01f && cameraTurn(block(12,4,0,0),block(12,4,5,0))>0.99f,"camera cuts found");
    std::cout<<"VIF1 blend tests passed\n";
    return 0;
}
