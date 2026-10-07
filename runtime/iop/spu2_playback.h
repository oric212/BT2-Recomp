#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>

// Authored sample-position/ADPCM flag model. Sound addresses count halfwords;
// one 16-byte ADPCM block describes 28 samples. No decoding, ADSR or mixing.
struct Spu2Playback {
    struct Voice {
        bool active=false,releasing=false,loop_known=false;
        std::uint32_t block=0,loop=0,pitch=0,position=0;
    };
    bool enabled=false;
    std::array<std::array<Voice,24>,2> voices{};
    std::array<std::uint32_t,2> endx{};
    std::uint64_t sample_clock_tenths=0,samples=0,blocks=0;

    void keyOn(unsigned core,std::uint32_t mask,const std::map<std::uint32_t,std::uint32_t>& registers) {
        auto next=voices.at(core);
        for(unsigned n=0;n<24;++n)if(mask&(1u<<n)) {
            const auto base=0x1F900000u+core*0x400;
            auto parameter=[&](unsigned offset) {
                const auto found=registers.find(base+offset);
                if(found==registers.end())throw std::runtime_error("SPU2 key-on lacks owned voice parameters");
                return found->second;
            };
            Voice v;v.active=true;v.pitch=parameter(n*16+4)&0x3FFF;
            v.block=((parameter(0x1C0+n*12)&0xF)<<16)|(parameter(0x1C2+n*12)&0xFFF8);
            const auto high=registers.find(base+0x1C4+n*12),low=registers.find(base+0x1C6+n*12);
            if(high!=registers.end() && low!=registers.end()) {v.loop_known=true;v.loop=((high->second&0xF)<<16)|(low->second&0xFFF8);}
            next[n]=v;
        }
        voices[core]=next;endx[core]&=~(mask&0xFFFFFF);
    }
    void keyOff(unsigned core,std::uint32_t mask) {
        for(unsigned n=0;n<24;++n)if((mask&(1u<<n)) && voices.at(core)[n].active)voices[core][n].releasing=true;
        // Release duration is ADSR-dependent. Keep position advancing; ENVX remains unsupported.
    }
    void advanceTenths(std::uint64_t clocks,const std::map<std::uint32_t,std::uint16_t>& memory) {
        if(!enabled)return;
        // 36.864 MHz / 48 kHz = 768 IOP clocks per output sample.
        if(clocks>UINT64_MAX-sample_clock_tenths)throw std::runtime_error("SPU2 sample clock overflow");
        const auto total=clocks+sample_clock_tenths,elapsed=total/7680;
        auto next=voices;auto flags=endx;std::uint64_t walked=0;
        for(unsigned core=0;core<2;++core)for(unsigned n=0;n<24;++n) {
            auto& v=next[core][n];if(!v.active || !elapsed)continue;
            if(elapsed>(UINT64_MAX-v.position)/0x3FFF)throw std::runtime_error("SPU2 voice position overflow");
            std::uint64_t position=v.position+elapsed*v.pitch;
            for(;;) {
                const auto header=memory.find(v.block);
                if(header==memory.end())throw std::runtime_error("SPU2 playback reads unowned ADPCM block");
                const auto block_flags=header->second>>8;
                if(block_flags&4) {v.loop=v.block;v.loop_known=true;}
                if(position<28u*4096)break;
                position-=28u*4096;++walked;
                if(walked>1000000)throw std::runtime_error("SPU2 playback block budget exceeded");
                if(block_flags&1) {
                    flags[core]|=1u<<n;
                    if(!(block_flags&2)) {v.active=false;position=0;break;}
                    if(!v.loop_known)throw std::runtime_error("SPU2 repeat lacks owned loop address");
                    v.block=v.loop;
                } else v.block=(v.block+8)&0xFFFFF;
                // Check a new block only once its first sample is consumed.
                if(!position)break;
            }
            v.position=static_cast<std::uint32_t>(position);
        }
        voices=next;endx=flags;sample_clock_tenths=total%7680;samples+=elapsed;blocks+=walked;
    }
};
