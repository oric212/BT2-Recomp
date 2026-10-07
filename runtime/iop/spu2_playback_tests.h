#pragma once
#include "native_runtime.h"

inline void testSpu2Playback() {
    auto require=[](bool ok){if(!ok)throw std::runtime_error("SPU2 playback fixture differs");};
    auto reject=[&](auto action){bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed);};
    auto registers=[](unsigned pitch=4096,unsigned address=0x100,unsigned core=0,unsigned voice=0) {
        const auto base=0x1F900000u+core*0x400;
        return std::map<std::uint32_t,std::uint32_t>{{base+voice*16+4,pitch},
            {base+0x1C0+voice*12,address>>16},{base+0x1C2+voice*12,address&0xFFFF}};
    };
    std::map<std::uint32_t,std::uint16_t> data{{0x100,0x0100}};
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
        p.advanceTenths(28*7680,loop);require(p.voices[0][0].block==0x108 && p.blocks==3);
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
        reject([&]{rt.read(0xBF90000A,2);}); // ENVX is still checked, not synthesized.
        rt.spu2_reset_status_reads=2;rt.write(0xBF9001A4,1,2);require(rt.spu2_playback.voices[0][0].releasing);
    }
}
