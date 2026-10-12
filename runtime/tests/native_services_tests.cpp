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

    {
        // Audio flow as logged from the original: two sources of one player, data announced each field,
        // and after the start the played blocks per field are 2,1,2,1,2,1,2,2,1,2,1,2,1,2,2,...
        CriAudioFlow flow;flow.sourceCreated(0xA,0x1A,0x3659C8);flow.sourceCreated(0xB,0x1B,0x3659DC);flow.playerCreated(0x50,0x1A,0x1B);
        std::vector<std::uint8_t> buffer(cri_stream_size);
        const auto word=[&](std::size_t offset,std::uint32_t value){std::memcpy(buffer.data()+offset,&value,4);};
        const auto at=[&](std::size_t offset){std::uint32_t value;std::memcpy(&value,buffer.data()+offset,4);return value;};
        std::uint32_t write_a=0x179140,write_b=0x17D240;
        const auto announce=[&](std::uint32_t bytes) {
            word(0,2);word(0x10,0x100);word(0x14,0xA);word(0x18,write_a);word(0x1C,bytes);word(0x20,0x100);word(0x24,0xB);word(0x28,write_b);word(0x2C,bytes);
            consumeCriStreamCommands(buffer,&flow,1);write_a+=bytes;write_b+=bytes;
        };
        announce(0xA00);flow.field();require(flow.played(1).empty(),"no messages before the start");
        std::vector<std::uint8_t> command(cri_stream_size);const std::uint32_t play[5]={1,0,0,0,2};
        std::memcpy(command.data(),play,4);std::memcpy(command.data()+0x10,&play[4],4);const std::uint32_t args[2]={0x50,1};std::memcpy(command.data()+0x14,args,8);
        consumeCriStreamCommands(command,&flow,0);
        const unsigned expected[]={2,1,2,1,2,1,2,2,1,2,1,2,1,2,2,1,2,1,2,1,2,1,2,2};
        std::uint32_t read_a=0x179140;
        for(const auto blocks:expected) {
            announce(0xA00);flow.field();
            const auto messages=flow.played(1);
            require(messages.size()==2 && messages[0].ee_object==0x3659C8 && messages[0].address==read_a && messages[0].bytes==blocks*0x400
                && messages[1].ee_object==0x3659DC && messages[1].address==read_a+0x4100 && messages[1].bytes==blocks*0x400,"played blocks per field");
            read_a+=blocks*0x400;
            writeCriStreamMessages(buffer,messages);
            require(at(0)==2 && at(0x10)==0 && at(0x14)==0x3659C8 && at(0x1C)==blocks*0x400 && at(0x24)==0x3659DC,"messages in the returned buffer");
        }
        // Writes that are not contiguous (the ring wraps) are reported as separate stretches.
        CriAudioFlow wrap;wrap.sourceCreated(1,0x11,0x100);wrap.sourceCreated(2,0x12,0x200);wrap.playerCreated(9,0x11,0x12);
        wrap.dataWritten(1,0x17CD40,0x400,1);wrap.dataWritten(1,0x179140,0x400,1);wrap.dataWritten(2,0x180E40,0x400,1);wrap.dataWritten(2,0x17D240,0x400,1);
        wrap.play(9,1);wrap.field();
        const auto split=wrap.played(1);
        require(split.size()==4 && split[0].address==0x17CD40 && split[1].address==0x179140 && split[2].ee_object==0x200 && split[3].address==0x17D240,"wrapped stretches");
        // A player without announced data reports nothing; later data plays at the block rate from then on.
        wrap.field();require(wrap.played(1).empty(),"a starved player reports nothing");
        wrap.dataWritten(1,0x179540,0xA00,1);wrap.dataWritten(2,0x17D640,0xA00,1);wrap.field();
        const auto resumed=wrap.played(1);
        require(resumed.size()==2 && resumed[0].address==0x179540 && resumed[0].bytes<=0x800 && resumed[0].bytes==resumed[1].bytes,"data after starvation");
        // Stop ends playback and reports everything still announced, split where the ring wraps (as
        // logged: {17C540, C00} then {179140, 2000}); other arguments are refused.
        CriAudioFlow stop;stop.sourceCreated(1,0x11,0x100);stop.playerCreated(9,0x11,0x12);stop.play(9,1);
        stop.dataWritten(1,0x17C540,0xC00,1);stop.dataWritten(1,0x179140,0x2000,1);stop.play(9,0);stop.field();
        const auto flushed=stop.played(1);
        require(flushed.size()==2 && flushed[0].address==0x17C540 && flushed[0].bytes==0xC00 && flushed[1].address==0x179140 && flushed[1].bytes==0x2000,"stop reports the remaining data");
        stop.field();require(stop.played(1).empty(),"a stopped player reports nothing more");
        wrap.play(9,0);
        bool playing_destroy=false;wrap.play(9,1);try{wrap.playerDestroyed(9);}catch(const std::runtime_error&){playing_destroy=true;}
        wrap.play(9,0);wrap.playerDestroyed(9);
        bool gone=false;try{wrap.play(9,1);}catch(const std::runtime_error&){gone=true;}
        require(playing_destroy && gone,"only a stopped player is destroyed");
        wrap.playerCreated(9,0x11,0x12);
        bool refused_argument=false;try{wrap.play(9,2);}catch(const std::runtime_error&){refused_argument=true;}
        require(refused_argument,"unmodelled player argument refused");
    }
    std::cout<<"Native IOP heap, pad block and CRI stream checks passed."<<std::endl;
}
