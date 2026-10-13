#include "services/audio_mixer.h"
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
        {
            // A 24 kHz player (command 4) plays half-size blocks at the same block rate, as logged:
            // 0x400, 0x200, 0x400, 0x200 ... bytes per field.
            CriAudioFlow slow;slow.sourceCreated(1,0x11,0x365900);slow.playerCreated(9,0x11,0x12);
            std::vector<std::uint8_t> setup(cri_stream_size);const std::uint32_t records[8]={4,9,24000,0xFFFFFFF1u,2,9,1,0};
            const std::uint32_t two=2;std::memcpy(setup.data(),&two,4);std::memcpy(setup.data()+0x10,records,32);
            consumeCriStreamCommands(setup,&slow,0);
            slow.dataWritten(1,0x150740,0x2000,1);
            const unsigned halves[]={0x400,0x200,0x400,0x200};
            for(const auto bytes:halves) {slow.field();const auto m=slow.played(1);require(m.size()==1 && m[0].bytes==bytes,"24 kHz blocks");}
            bool odd=false;try{slow.rate(9,44100);}catch(const std::runtime_error&){odd=true;}
            require(odd,"unmodelled rate refused");
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
        {
            // Sound output: played blocks are passed on with source, side and rate; what a stop only
            // reports is not, and a destroyed player silences its sources.
            CriAudioFlow heard;heard.sourceCreated(1,0x11,0x100);heard.sourceCreated(2,0x12,0x200);heard.playerCreated(9,0x11,0x12);heard.rate(9,24000);
            struct Piece {std::uint32_t source;unsigned side;std::uint32_t hertz,address,bytes;};std::vector<Piece> pieces;std::vector<std::uint32_t> silenced;
            heard.on_audible=[&](std::uint32_t source,unsigned side,std::uint32_t hertz,std::uint32_t address,std::uint32_t bytes){pieces.push_back({source,side,hertz,address,bytes});};
            heard.on_silenced=[&](std::uint32_t source){silenced.push_back(source);};
            heard.dataWritten(1,0x1000,0x1000,1);heard.dataWritten(2,0x3000,0x1000,1);heard.play(9,1);heard.field();
            const auto first=heard.played(1);
            require(first.size()==2 && pieces.size()==2 && pieces[0].source==1 && pieces[0].side==0 && pieces[0].hertz==24000 && pieces[0].address==0x1000
                && pieces[0].bytes==first[0].bytes && pieces[1].source==2 && pieces[1].side==1 && pieces[1].address==0x3000,"audible stretches");
            heard.play(9,0);heard.field();
            require(heard.played(1).size()==2 && pieces.size()==2,"a stop's report is not heard");
            heard.play(9,1);heard.dataWritten(1,0x1000,0x400,1);heard.playerDestroyed(9);
            require(silenced==std::vector<std::uint32_t>{1,2},"a destroyed player silences its sources");
        }
        {
            // Mixer: a voice starts with one block of silence; 24 kHz is doubled with interpolation;
            // sides are routed left and right; five fields give 4,004 samples; a dry voice counts
            // its missing samples and starts over.
            AudioMixer mixer;
            std::vector<std::uint8_t> pcm(4000);
            for(std::size_t n=0;n<pcm.size();n+=2){const std::int16_t v=std::int16_t(1000+n);std::memcpy(pcm.data()+n,&v,2);}
            mixer.push(7,0,48000,pcm);
            std::vector<std::uint8_t> slow(4);const std::int16_t pair[2]={200,400};std::memcpy(slow.data(),pair,4);
            mixer.push(8,1,24000,slow);
            const auto out=mixer.field();
            require(out.size()==800*2 && out[0]==0 && out[511*2]==0 && out[512*2]==390 && out[513*2]==391,"left voice after its silent block, at the stream level");
            require(out[511*2+1]==0 && out[512*2+1]==39 && out[513*2+1]==78 && out[514*2+1]==117 && out[515*2+1]==156 && out[516*2+1]==0,"right voice doubled with interpolation");
            require(mixer.underruns==800-516,"the short voice ran dry");
            std::size_t samples=out.size()/2;for(unsigned n=0;n<4;++n)samples+=mixer.field().size()/2;
            require(samples==4004,"five fields are 4,004 samples");
            std::vector<std::uint8_t> loud(2);const std::int16_t top=30000;std::memcpy(loud.data(),&top,2);
            AudioMixer sum;sum.push(1,0,48000,loud);sum.push(2,0,48000,loud);
            AudioMixer three;for(unsigned key=1;key<=3;++key)three.push(key,0,48000,loud);
            require(sum.field()[512*2]==2*(30000*25/64) && three.field()[512*2]==32767,"voices on one side add and saturate");
            bool refused=false;try{sum.push(3,0,44100,loud);}catch(const std::runtime_error&){refused=true;}
            require(refused,"a rate that does not divide 48 kHz is refused");
            sum.drop(1);
        }
        wrap.play(9,0);
        // A player destroyed while playing (a skipped movie) ends without reports; a queued stop for it
        // is ignored, a start for it refused.
        wrap.play(9,1);wrap.dataWritten(1,0x179540,0x800,1);wrap.playerDestroyed(9);wrap.field();
        require(wrap.played(1).empty(),"a destroyed player reports nothing");
        wrap.play(9,0);
        bool gone=false;try{wrap.play(9,1);}catch(const std::runtime_error&){gone=true;}
        require(gone,"a destroyed player cannot start");
        wrap.playerCreated(9,0x11,0x12);
        bool refused_argument=false;try{wrap.play(9,2);}catch(const std::runtime_error&){refused_argument=true;}
        require(refused_argument,"unmodelled player argument refused");
    }
    std::cout<<"Native IOP heap, pad block and CRI stream checks passed."<<std::endl;
}
