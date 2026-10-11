#include "services/iop_heap.h"
#include "services/pad_block.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

// Native service logic that runs without game code: IOP heap placement and the pad block layout.
// The full request paths run in the startup probe.

namespace {
void require(bool condition,const char* what) {
    if(!condition){std::cerr<<"native services test failed: "<<what<<std::endl;std::exit(1);}
}
}

int main() {
    // Heap: the captured allocation/free sequence of the original boot heap.
    IopHeap heap;
    const auto first=heap.allocate(0x360);
    require(first==0xCE500,"first allocation");
    require(heap.release(first) && !heap.release(first),"free and double free");
    const std::vector<std::pair<std::uint32_t,std::uint32_t>> sequence={
        {0x81800,0xCE500},{0x8D0,0x14FD00},{0x41040,0x150600},{0x8D0,0x191700},{0x4040,0x192000},{0x360,0x196100}};
    for(const auto& [size,expected]:sequence)require(heap.allocate(size)==expected,"captured placement");
    require(heap.release(0x196100),"reload block free");
    require(heap.allocate(0x2C0)==0x196100 && heap.allocate(0x180)==0x196400,"bank blocks reuse the freed block");
    require(heap.covers(0xCE500,0x16640) && !heap.covers(0x196400,0x201) && !heap.covers(0xCE400,16),"upload coverage");
    require(!heap.allocate(0) && !heap.allocate(0x200000),"empty and oversized requests");

    // Pad block: first transfer zero; neutral, Left and Cross as DBCMAN wrote them.
    DualShock2 pad;
    for(auto w:nativePadBlock(pad,0))require(!w,"first transfer is zero");
    auto block=nativePadBlock(pad,1);
    require(block[0]==0x04121601 && block[1]==1 && block[7]==0x8080FFFF && block[8]==0x8080 && block[9]==0 && block[10]==0
        && block[11]==0xFFFF0000 && block[12]==0xFFFF && block[31]==1,"neutral block");
    pad.buttons=0xFF7F; // Left.
    block=nativePadBlock(pad,2);
    require(block[7]==0x8080FF7F && block[8]==0xFF008080 && block[31]==2,"left block");
    pad.buttons=0xBFFF; // Cross.
    block=nativePadBlock(pad,3);
    require(block[7]==0x8080BFFF && block[8]==0x8080 && block[10]==0xFF,"cross block");

    std::cout<<"Native IOP heap and pad block checks passed."<<std::endl;
}
