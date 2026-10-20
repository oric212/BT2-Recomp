// In-between VIF1 lists (runtime/vu/vif1_blend.h) on synthetic lists (no game data).
#include "vu/vif1_blend.h"
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
    check(mid.has_value() && blended==2,"two floats blended");
    check(value((*mid)[2])==2.0f && value((*mid)[6])==15.0f,"halfway values");
    check((*mid)[3]==0x00000007u,"a word that is not a normal float is the newer one");
    check((*mid)[12]==0x2222u,"DIRECT data is the newer frame's");
    check((*mid)[4]==bits(1.0f) && (*mid)[17]==0x00020001u,"equal words and other formats unchanged");
    // Lists that do not pair: another number of unpacks, or another size.
    auto shorter=newer;shorter.resize(16);
    check(!blendVif1(older,shorter,0.5f),"a missing unpack does not pair");
    auto resized=newer;resized[1]=0x6C010000u;resized.erase(resized.begin()+6,resized.begin()+10);
    check(!blendVif1(older,resized,0.5f),"an unpack of another size does not pair");
    // STCYCL with WL > CL: the data covers CL of every WL elements.
    const std::vector<std::uint32_t> skipping{0x01000201u,0x6C040000u,1,2,3,4,5,6,7,8};
    const auto u=vif1Unpacks(skipping);
    check(u && u->size()==1 && (*u)[0].words==8,"WL > CL sizes the data by CL");
    check(!vif1Unpacks(std::vector<std::uint32_t>{0x6C020000u,1,2}),"data past the end is refused");
    std::cout<<"VIF1 blend tests passed\n";
    return 0;
}
