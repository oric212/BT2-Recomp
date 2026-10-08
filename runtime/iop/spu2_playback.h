#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>

// Authored sample-position/ADPCM flag and ADSR envelope model. Sound addresses count halfwords;
// one 16-byte ADPCM block describes 28 samples. No decoding or mixing.
//
// Envelope: ADSR1 = attack exponential (15), attack shift (14-10), attack step (9-8), decay shift
// (7-4), sustain level (3-0); ADSR2 = sustain exponential (15), sustain decrease (14), sustain shift
// (12-8), sustain step (7-6), release exponential (5), release shift (4-0) (public SPU register
// layout). Each output sample advances a rate counter by 0x8000 >> max(0, shift - 11); on overflow the
// level moves by step << max(0, 11 - shift), scaled by the level for exponential decrease and with a
// quarter rate above 0x6000 for exponential increase (as PCSX2's ADSR). ENVX reads the level.
struct Spu2Envelope {
    enum Phase:unsigned {Stopped,Attack,Decay,Sustain,Release};
    unsigned phase=Stopped;std::uint32_t counter=0;std::int32_t level=0;
    std::uint16_t adsr1=0,adsr2=0;
    struct Rate {bool decrease,exponential;int shift,step;std::int32_t target;};
    Rate rate() const {
        switch(phase) {
        case Attack:return {false,(adsr1>>15)!=0,(adsr1>>10)&31,7-((adsr1>>8)&3),0x7FFF};
        case Decay:return {true,true,(adsr1>>4)&15,-8,((adsr1&15)+1)<<11};
        case Sustain: {
            const bool down=(adsr2>>14)&1;const int step=7-((adsr2>>6)&3);
            return {down,(adsr2>>15)!=0,(adsr2>>8)&31,down?~step:step,0};
        }
        default:return {true,((adsr2>>5)&1)!=0,adsr2&31,-8,0};
        }
    }
    void attack(std::uint16_t a1,std::uint16_t a2) {adsr1=a1;adsr2=a2;phase=Attack;counter=0;level=0;}
    void release() {if(phase!=Stopped){phase=Release;counter=0;}}
    void stop() {phase=Stopped;counter=0;level=0;}
    // One output sample; false once the envelope has finished (the voice stops).
    bool step() {
        if(phase==Stopped)return false;
        const auto r=rate();
        std::uint32_t counter_step=0x8000u>>std::max(0,r.shift-11);
        std::int32_t level_step=r.step<<std::max(0,11-r.shift);
        if(r.exponential) {
            if(!r.decrease && level>0x6000)counter_step>>=2;
            if(r.decrease)level_step=static_cast<std::int16_t>((level_step*level)>>15);
        }
        counter+=std::max<std::uint32_t>(1,counter_step);
        if(counter>=0x8000) {counter=0;level=std::clamp<std::int32_t>(level+level_step,0,0x7FFF);}
        if(phase==Sustain)return level!=0;
        if((!r.decrease && level>=r.target) || (r.decrease && level<=r.target))++phase;
        if(phase>Release) {stop();return false;}
        return true;
    }
};

struct Spu2Playback {
    struct Voice {
        bool active=false,releasing=false,loop_known=false,loop_manual=false;
        std::uint32_t block=0,loop=0,pitch=0,position=0;
        Spu2Envelope envelope;
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
            v.envelope.attack(static_cast<std::uint16_t>(parameter(n*16+6)),static_cast<std::uint16_t>(parameter(n*16+8)));
            v.block=((parameter(0x1C0+n*12)&0xF)<<16)|(parameter(0x1C2+n*12)&0xFFF8);
            const auto high=registers.find(base+0x1C4+n*12),low=registers.find(base+0x1C6+n*12);
            if(high!=registers.end() && low!=registers.end()) {v.loop_known=true;v.loop=((high->second&0xF)<<16)|(low->second&0xFFF8);}
            next[n]=v;
        }
        voices[core]=next;endx[core]&=~(mask&0xFFFFFF);
    }
    // A voice register written while the voice plays: pitch and ADSR take effect at once; a loop
    // address written after key-on overrides the blocks' loop-start flags until the next key-on.
    void registerWritten(unsigned core,unsigned offset,const std::map<std::uint32_t,std::uint32_t>& registers) {
        const auto base=0x1F900000u+core*0x400;
        if(offset<0x180) {
            auto& v=voices.at(core).at(offset/16);
            const auto value=registers.at(base+offset);
            if(offset%16==4)v.pitch=value&0x3FFF;
            if(offset%16==6)v.envelope.adsr1=static_cast<std::uint16_t>(value);
            if(offset%16==8)v.envelope.adsr2=static_cast<std::uint16_t>(value);
            return;
        }
        if(offset<0x1C0 || offset>=0x2E0)return;
        const unsigned n=(offset-0x1C0)/12,field=(offset-0x1C0)%12;auto& v=voices.at(core).at(n);
        if((field!=4 && field!=6) || !v.active)return;
        const auto high=registers.find(base+0x1C4+n*12),low=registers.find(base+0x1C6+n*12);
        const auto current_high=v.loop_known?(v.loop>>16)&0xF:0u,current_low=v.loop_known?v.loop&0xFFF8:0u;
        // Each half replaces only its own bits, as the hardware register halves do.
        const auto h=field==4?(high->second&0xF):current_high,l=field==6?(low->second&0xFFF8):current_low;
        v.loop=(h<<16)|l;v.loop_known=true;v.loop_manual=true;
    }
    void keyOff(unsigned core,std::uint32_t mask) {
        for(unsigned n=0;n<24;++n)if((mask&(1u<<n)) && voices.at(core)[n].active) {voices[core][n].releasing=true;voices[core][n].envelope.release();}
    }
    void advanceTenths(std::uint64_t clocks,const std::map<std::uint32_t,std::uint16_t>& memory) {
        if(!enabled)return;
        // 36.864 MHz / 48 kHz = 768 IOP clocks per output sample.
        if(clocks>UINT64_MAX-sample_clock_tenths)throw std::runtime_error("SPU2 sample clock overflow");
        const auto total=clocks+sample_clock_tenths,elapsed=total/7680;
        auto next=voices;auto flags=endx;std::uint64_t walked=0;
        for(unsigned core=0;core<2;++core)for(unsigned n=0;n<24;++n) {
            auto& v=next[core][n];if(!v.active || !elapsed)continue;
            // The envelope runs first: a voice whose envelope finishes stops at that sample.
            std::uint64_t played=0;
            while(played<elapsed && v.envelope.step())++played;
            const bool silenced=played<elapsed;if(silenced)++played;
            if(played>(UINT64_MAX-v.position)/0x3FFF)throw std::runtime_error("SPU2 voice position overflow");
            std::uint64_t position=v.position+played*v.pitch;
            for(;;) {
                const auto header=memory.find(v.block);
                if(header==memory.end())throw std::runtime_error("SPU2 playback reads unowned ADPCM block");
                const auto block_flags=header->second>>8;
                if((block_flags&4) && !v.loop_manual) {v.loop=v.block;v.loop_known=true;}
                if(position<28u*4096)break;
                position-=28u*4096;++walked;
                if(walked>1000000)throw std::runtime_error("SPU2 playback block budget exceeded");
                if(block_flags&1) {
                    flags[core]|=1u<<n;
                    if(!(block_flags&2)) {v.active=false;v.envelope.stop();position=0;break;}
                    if(!v.loop_known)throw std::runtime_error("SPU2 repeat lacks owned loop address");
                    v.block=v.loop;
                } else v.block=(v.block+8)&0xFFFFF;
                // Check a new block only once its first sample is consumed.
                if(!position)break;
            }
            v.position=static_cast<std::uint32_t>(position);
            if(silenced && v.active) {v.active=false;v.position=0;}
        }
        voices=next;endx=flags;sample_clock_tenths=total%7680;samples+=elapsed;blocks+=walked;
    }
};
