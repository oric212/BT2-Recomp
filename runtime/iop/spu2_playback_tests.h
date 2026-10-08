#pragma once
#include "native_runtime.h"
#include <source_location>
#include <string>

inline void testSpu2Playback() {
    auto require=[](bool ok,std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("SPU2 playback fixture differs at line "+std::to_string(at.line()));};
    auto reject=[&](auto action,std::source_location at=std::source_location::current()){bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,at);};
    auto registers=[](unsigned pitch=4096,unsigned address=0x100,unsigned core=0,unsigned voice=0) {
        const auto base=0x1F900000u+core*0x400;
        return std::map<std::uint32_t,std::uint32_t>{{base+voice*16+4,pitch},{base+voice*16+6,0},{base+voice*16+8,0},
            {base+0x1C0+voice*12,address>>16},{base+0x1C2+voice*12,address&0xFFFF}};
    };
    std::map<std::uint32_t,std::uint16_t> data{{0x100,0x0100}};
    {
        // ADSR1/ADSR2 = 0: fastest linear attack, exponential decay to sustain level 0x800, linear sustain
        // increase back to full; release (exponential flag clear, shift 0) falls 0x4000 per sample.
        Spu2Envelope e;e.attack(0,0);
        require(e.step() && e.level==0x3800 && e.phase==Spu2Envelope::Attack);
        require(e.step() && e.level==0x7000 && e.step() && e.level==0x7FFF && e.phase==Spu2Envelope::Decay);
        unsigned guard=0;while(e.phase==Spu2Envelope::Decay && guard++<64)require(e.step());
        require(e.phase==Spu2Envelope::Sustain && e.level<=0x800);
        for(unsigned n=0;n<8;++n)require(e.step());
        require(e.level==0x7FFF && e.phase==Spu2Envelope::Sustain);
        e.release();require(e.step() && e.level==0x3FFF && !e.step() && e.phase==Spu2Envelope::Stopped && !e.level);
        // Slow linear attack (shift 20: one level step every 2^9 samples) and exponential slow-down above 0x6000.
        Spu2Envelope slow;slow.attack(20u<<10,0);
        for(unsigned n=0;n<511;++n)require(slow.step());
        require(!slow.level && slow.step() && slow.level==7);
        Spu2Envelope curve;curve.attack(0x8000u|(11u<<10),0);curve.level=0x6001;curve.counter=0;
        for(unsigned n=0;n<3;++n)require(curve.step());
        require(curve.level==0x6001 && curve.step() && curve.level==0x6008);
    }
    {
        // Key-off releases; the finished envelope stops the voice and its position, without ENDX.
        Spu2Playback p;p.enabled=true;p.keyOn(0,1,registers());
        p.advanceTenths(16*7680,data);require(p.voices[0][0].envelope.level==0x7FFF && p.voices[0][0].active);
        p.keyOff(0,1);p.advanceTenths(4*7680,data);
        require(!p.voices[0][0].active && !p.voices[0][0].envelope.level && !(p.endx[0]&1) && !p.voices[0][0].position);
        // An ADPCM end without repeat stops the envelope too.
        p.keyOn(0,1,registers());p.advanceTenths(28*7680,data);require(!p.voices[0][0].active && !p.voices[0][0].envelope.level && (p.endx[0]&1));
    }
    {
        Spu2Playback p;p.enabled=true;p.endx[0]=0xFFFFFF;p.keyOn(0,1,registers());
        require(p.endx[0]==0xFFFFFE && p.voices[0][0].active);
        p.advanceTenths(27*7680,data);require(!(p.endx[0]&1) && p.voices[0][0].position==27*4096);
        p.advanceTenths(7680,data);require((p.endx[0]&1) && !p.voices[0][0].active && p.blocks==1);
        p.keyOn(0,1,registers());require(!(p.endx[0]&1) && p.voices[0][0].position==0);
    }
    {
        Spu2Playback p;p.enabled=true;p.keyOn(1,1u<<23,registers(2048,0x100,1,23));
        p.advanceTenths(55*7680,data);require(!p.endx[1] && p.voices[1][23].position==55*2048);
        p.advanceTenths(7680,data);require(p.endx[1]==(1u<<23) && !p.endx[0]);
    }
    {
        Spu2Playback p;p.enabled=true;p.keyOn(0,1,registers());
        const std::map<std::uint32_t,std::uint16_t> loop{{0x100,0x0400},{0x108,0x0300}};
        p.advanceTenths(56*7680,loop);require(p.endx[0]==1 && p.voices[0][0].active && p.voices[0][0].block==0x100);
        p.keyOff(0,1);require(p.voices[0][0].active && p.voices[0][0].releasing);
        p.advanceTenths(28*7680,loop);require(!p.voices[0][0].active && p.blocks==2); // ADSR2=0 release ends in 2 samples.
    }
    {
        Spu2Playback split,whole;split.enabled=whole.enabled=true;
        split.keyOn(0,1,registers());whole.keyOn(0,1,registers());
        split.advanceTenths(7679,data);require(!split.samples && !split.voices[0][0].position);
        split.advanceTenths(1,data);whole.advanceTenths(7680,data);
        require(split.samples==whole.samples && split.sample_clock_tenths==whole.sample_clock_tenths
            && split.voices[0][0].position==whole.voices[0][0].position);
    }
    {
        Spu2Playback p;p.enabled=true;p.keyOn(0,1,registers(0));
        p.advanceTenths(100*7680,data);require(p.voices[0][0].position==0 && !p.endx[0]);
        const auto before=p.voices[0][0];
        reject([&]{p.keyOn(0,3,registers());});require(p.voices[0][0].pitch==before.pitch); // Atomic command rejection.
        p.keyOn(0,1,registers());const auto samples=p.samples;
        reject([&]{p.advanceTenths(7680,{});});require(p.samples==samples && !p.voices[0][0].position);
        reject([&]{p.advanceTenths(28*7680,{{0x100,0x0300}});}); // Repeating without an owned LSA.
        require(p.samples==samples && !p.endx[0]);
    }
    {
        IopRuntime rt;rt.spu2_playback.enabled=true;rt.spu2_io_transfer_completed=true;
        rt.spu2_configuration=registers(0x3FFF,0x2800);
        for(unsigned voice=0;voice<24;++voice)for(unsigned side=0;side<2;++side)
            rt.spu2_configuration[0x1F900000+voice*16+side*2]=0;
        rt.spu2_sound_ram[0x2800]=0x0700;
        rt.write(0xBF9001A0,1,2);rt.spu2_playback.advanceTenths(8*7680,rt.spu2_sound_ram);
        rt.steps+=100;require(rt.read(0xBF900340,2)==1 && rt.read(0xBF900340,2)==1);
        rt.spu2_playback.endx[0]|=1u<<23;rt.write(0xBF900340,0xFFFF,2);
        require(rt.read(0xBF900340,2)==0 && rt.read(0xBF900342,2)==0x80);
        rt.write(0xBF900342,0,2);require(!rt.spu2_playback.endx[0]);
        // Live register writes: pitch and loop address follow at once; the loop address overrides block flags.
        rt.write(0xBF900004,0x1000,2);require(rt.spu2_playback.voices[0][0].pitch==0x1000);
        rt.write(0xBF9001C6,0x3000,2);require(rt.spu2_playback.voices[0][0].loop_manual && rt.spu2_playback.voices[0][0].loop==0x3000);
        require(rt.read(0xBF9001C6,2)==0x3000 && rt.read(0xBF9001C4,2)==0);
        rt.write(0xBF900170,0xC34,2);require(rt.spu2_configuration.at(0x1F900170)==0xC34); // Volume retained.
        // SH stores the low halfword of a 24-bit routing mask (voice 23 on: 0x0000 low, 0x80 high).
        rt.write(0xBF900188,0x800000,2);rt.write(0xBF90018A,0x80,2);
        require(rt.spu2_configuration.at(0x1F900188)==0 && rt.spu2_configuration.at(0x1F90018A)==0x80);
        reject([&]{rt.write(0xBF90018A,0x100,2);}); // The high half carries 8 voice bits.
        // ENVX reads the envelope level (8 samples after key-on: attack, decay, one sustain step).
        require(rt.read(0xBF90000A,2)==0x3FFF && rt.read(0xBF90001A,2)==0 && rt.read(0xBF90040A,2)==0);
        rt.spu2_reset_status_reads=2;rt.write(0xBF9001A4,1,2);require(rt.spu2_playback.voices[0][0].releasing);
    }
}
