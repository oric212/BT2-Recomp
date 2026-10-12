#include "services/iop_heap.h"
#include "services/pad_block.h"
#include "services/cri_stream.h"
#include <cstring>
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

    // CRI stream: the captured command pair (8 and 5 on one object) comes back with the count cleared
    // and the records unchanged; an empty buffer stays empty; unknown codes are refused.
    std::vector<std::uint8_t> stream(cri_stream_size);
    const auto put=[&](std::size_t offset,std::uint32_t value){std::memcpy(stream.data()+offset,&value,4);};
    put(0,2);put(0x10,8);put(0x14,0xC6D5C);put(0x18,1);put(0x20,5);put(0x24,0xC6D5C);put(0x2C,0x65);
    auto before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==2 && stream==before,"CRI commands consumed");
    require(consumeCriStreamCommands(stream)==0 && stream==before,"CRI empty buffer");
    // The movie audio start: 8, 4 (rate), 9 twice (pan) are echoed the same way.
    put(0,4);put(0x10,8);put(0x18,0);put(0x20,4);put(0x28,0xBB80);put(0x30,9);put(0x34,0xC6D5C);put(0x3C,0xFFFFFFF1);
    put(0x40,9);put(0x44,0xC6D5C);put(0x48,1);put(0x4C,0xF);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==4 && stream==before,"CRI audio start consumed");
    // Audio data: 0x100 with the IOP address and size, one record per audio channel.
    put(0,2);put(0x10,0x100);put(0x14,0xCD578);put(0x18,0x179140);put(0x1C,0xA00);put(0x20,0x100);put(0x24,0xCD58C);put(0x28,0x17D240);put(0x2C,0xA00);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==2 && stream==before,"CRI audio data consumed");
    put(0,1);put(0x10,2);put(0x14,0xC6D5C);put(0x18,1);put(0x1C,0);
    before=stream;before[0]=0;
    require(consumeCriStreamCommands(stream)==1 && stream==before,"CRI command 2 consumed");
    put(0,1);put(0x10,7);bool refused=false;
    try{consumeCriStreamCommands(stream);}catch(const std::runtime_error&){refused=true;}
    require(refused,"unknown CRI command refused");

    std::cout<<"Native IOP heap, pad block and CRI stream checks passed."<<std::endl;
}
