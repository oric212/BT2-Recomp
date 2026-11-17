// Stream audio mixer (docs/plans/sound/SOUND_OUTPUT_PLAN.md): the PCM the CRI player reports as played
// (cri_audio.h) becomes 48 kHz stereo output, one field at a time. Each voice is one CRI source, mono
// 16-bit, heard with the left and right gains of its channel (0..255) as the original's player mixes
// it: 255 passes the sample, another gain g gives sample*g>>8, and the streams' sum is clamped to
// +-0x7FFF before the effects join. The player hands data over in blocks of 512 output samples while a
// field takes 800.8, so a voice starts with one block of silence: its queue then never runs dry
// while the player keeps playing. Time is logical: one field per VBLANK.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

class AudioMixer {
public:
    static constexpr unsigned rate=48000,block_samples=512,unity_gain=255;
    unsigned underruns=0;            // Samples a started voice could not supply.
    // The player's volumes in percent: music and films (stereo streams), speech (one-channel streams),
    // the further sources (`extra`, the sound effects) and the whole mix. At 100 the output is the
    // original's level, sample for sample.
    unsigned music_percent=100,speech_percent=100,extra_percent=100,master_percent=100;
    std::uint64_t voice_samples=0;   // Output samples taken from voices.

    // The gain the original's player applies to a sample.
    static std::int32_t gained(std::int32_t sample,unsigned gain) {return gain==unity_gain?sample:(sample*std::int32_t(gain))>>8;}

    // `bytes` of 16-bit little-endian mono PCM at `hertz` for voice `key`, heard with gains `left` and
    // `right` (0..255); `speech` puts it under the speech volume.
    void push(std::uint32_t key,unsigned left,unsigned right,bool speech,unsigned hertz,std::span<const std::uint8_t> bytes) {
        if(left>unity_gain || right>unity_gain || !hertz || rate%hertz || bytes.size()%2)
            throw std::runtime_error("Audio voice format is not modelled: "+std::to_string(hertz)+" Hz, gains "+std::to_string(left)+"/"+std::to_string(right));
        auto& v=voices_[key];v.speech=speech;
        if(v.queue.empty() && !v.running) {v.queue.insert(v.queue.end(),block_samples,Frame{});v.running=true;}
        // Integer upsampling with linear interpolation from the voice's last sample.
        const unsigned factor=rate/hertz;
        for(std::size_t n=0;n<bytes.size();n+=2) {
            std::int16_t sample;std::memcpy(&sample,bytes.data()+n,2);
            for(unsigned k=1;k<=factor;++k) {
                const auto s=v.last+(std::int32_t(sample)-v.last)*std::int32_t(k)/std::int32_t(factor);
                v.queue.push_back({std::int16_t(gained(s,left)),std::int16_t(gained(s,right))});
            }
            v.last=sample;
        }
    }

    // One field of output: interleaved stereo. A field is 800.8 samples (48,000 Hz at 59.94 fields/s);
    // the fraction is carried, so five fields give 4,004 samples.
    // `extra` adds further sources (the effects sampler) to the field's mix before it is clamped.
    std::vector<std::int16_t> field(const std::function<void(std::span<std::int32_t>)>& extra={}) {
        ticks_+=4004;const std::size_t count=ticks_/5;ticks_%=5;
        std::vector<std::int32_t> mix(count*2);
        for(auto& [key,v]:voices_) {
            if(!v.running)continue;
            const auto have=std::min(count,v.queue.size());
            const std::int32_t percent=std::int32_t(v.speech?speech_percent:music_percent);
            for(std::size_t n=0;n<have;++n) {
                mix[n*2]+=v.queue[n][0]*percent/100;mix[n*2+1]+=v.queue[n][1]*percent/100;
            }
            v.queue.erase(v.queue.begin(),v.queue.begin()+std::ptrdiff_t(have));
            voice_samples+=have;
            // A voice that runs dry has stopped (or stalled): it starts again with a block of silence.
            if(have<count) {v.running=false;v.last=0;underruns+=unsigned(count-have);}
        }
        for(auto& sample:mix)sample=std::clamp(sample,-0x7FFF,0x7FFF); // The player's own clamp.
        if(extra && extra_percent==100)extra(mix);
        else if(extra) {
            std::vector<std::int32_t> added(mix.size());extra(added);
            for(std::size_t n=0;n<mix.size();++n)mix[n]+=added[n]*std::int32_t(extra_percent)/100;
        }
        if(master_percent!=100)for(auto& sample:mix)sample=std::int32_t(std::int64_t(sample)*master_percent/100);
        std::vector<std::int16_t> out(mix.size());
        for(std::size_t n=0;n<mix.size();++n)out[n]=std::int16_t(std::clamp(mix[n],-32768,32767));
        return out;
    }

    // A voice whose source is gone: queued samples are dropped.
    void drop(std::uint32_t key) {voices_.erase(key);}

private:
    using Frame=std::array<std::int16_t,2>; // A sample with its gains applied: left, right.
    struct Voice {bool running=false,speech=false;std::int16_t last=0;std::deque<Frame> queue;};
    std::map<std::uint32_t,Voice> voices_;
    unsigned ticks_=0;
};
