// Native CRI ADX audio flow (docs/plans/video/SOFDEC_PLAN.md): what the IOP's player reports back
// while a stream plays. From a PCSX2 log of the audio channel (capture_ee.py --cri-stream-log,
// 2026-10-12): the EE announces PCM it wrote to IOP memory (command 0x100 {source, address, bytes}),
// and once the player is started (command 2 {player, 1}) each returned buffer carries one message per
// played stretch, {0, EE object, address, bytes}, in the order the data was written; a stop (command 2
// {player, 0}) reports the rest at once. The IOP plays
// in blocks of 512 output samples at 48 kHz (10.67 ms): 0x400 bytes per source of 48 kHz 16-bit data,
// 0x200 bytes of 24 kHz data (command 4 {player, rate, ...} sets the rate; both rates logged). A field
// (16.68 ms) reports the blocks that ended in it. No sound is produced here; time is logical (one field
// per VBLANK).
// What is heard: the original's player mixes its channels itself before the sound chip's input
// (CRI_ADXI's mixer, read from the module 2026-11-16). Each channel has a left and a right gain of
// 0..255 from the player's volume (command 5) and the channel's pan (command 9); 255 passes the
// sample unchanged, any other gain g gives sample*g>>8, and the sum of all channels is clamped to
// +-0x7FFF. A new player has volume 255, pans 0 and as many channels as buffers; command 3 sets the
// number of channels the game decodes (1 for speech).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class CriAudioFlow {
public:
    struct Message {std::uint32_t ee_object=0,address=0,bytes=0;};
    // A stretch of PCM a playing player consumed: its source, the channel's gains (0..255, see above),
    // whether the player is a one-channel stream (speech), the sampling rate and where the data lies.
    struct Heard {std::uint32_t source=0;unsigned left=0,right=0;bool speech=false;std::uint32_t hertz=0,address=0,bytes=0;};
    unsigned played_blocks=0,announced_bytes=0,commands_for_destroyed_players=0;
    // Sound output: each stretch heard. Data a stop only reports back is not heard and is not passed
    // on. `on_silenced` names a source whose player was destroyed.
    std::function<void(const Heard&)> on_audible;
    std::function<void(std::uint32_t source)> on_silenced;
    // Evidence: bytes heard per distinct {channels, volume, pan of the channel, rate} (at most 256 kinds).
    std::map<std::array<int,4>,std::uint64_t> mixes;
    // Evidence for the stream commands that are not decoded yet: each distinct {code, argument 1,
    // argument 2} other than a data announcement, with its count (at most 256 kinds).
    std::map<std::array<std::uint32_t,3>,unsigned> commands;
    void note(std::uint32_t code,std::uint32_t first,std::uint32_t second) {
        const std::array<std::uint32_t,3> key{code,first,second};
        if(commands.size()<256 || commands.count(key))++commands[key];
    }

    // RPC 0x400 created a source (one audio channel) on an IOP buffer object for an EE object; RPC 0x408
    // a player over two buffer objects.
    void sourceCreated(std::uint32_t handle,std::uint32_t buffer,std::uint32_t ee_object) {sources_[handle].buffer=buffer;sources_[handle].ee_object=ee_object;}
    void playerCreated(std::uint32_t handle,std::uint32_t first,std::uint32_t second) {players_[handle]=Player{{first,second}};destroyed_.erase(handle);}
    // Stream command 4: the player's sampling rate. A block is 0x400 bytes at 48 kHz and scales with the rate.
    void rate(std::uint32_t player,std::uint32_t hertz) {
        if(!hertz || hertz>48000 || (std::uint64_t(block_bytes)*hertz)%48000)throw std::runtime_error("CRI player rate "+std::to_string(hertz)+" is not modelled");
        known(players_,player,"CRI rate command for an unknown player").block=std::uint32_t(std::uint64_t(block_bytes)*hertz/48000);
    }

    // Stream command 5 {player, .., volume}: the player's volume, clamped to 0..255 as the original does
    // (256 is sent). Command 9 {player, channel, pan}: a channel's pan, clamped to -15..15. Command 3
    // {player, channels}: the number of channels. Each is dropped for a player already destroyed.
    void volume(std::uint32_t player,std::uint32_t value) {
        if(auto* p=live(player,"CRI volume command for an unknown player"))p->volume=std::int32_t(value)<0?0u:std::min(value,255u);
    }
    void pan(std::uint32_t player,std::uint32_t channel,std::uint32_t value) {
        if(channel>1)throw std::runtime_error("CRI pan command for channel "+std::to_string(channel)+" is not modelled");
        if(auto* p=live(player,"CRI pan command for an unknown player"))p->pan[channel]=std::clamp(std::int32_t(value),-15,15);
    }
    void channels(std::uint32_t player,std::uint32_t count) {
        if(count<1 || count>2)throw std::runtime_error("CRI channel count "+std::to_string(count)+" is not modelled");
        if(auto* p=live(player,"CRI channel command for an unknown player"))p->channels=count;
    }
    // The left and right gain of a channel at `volume` and `pan`: the volume times a constant-power pan
    // law of 255*cos((pan+15)*pi/60), rounded down, over 255 and rounded half up (the module's arithmetic).
    static std::array<unsigned,2> gains(unsigned volume,int pan) {
        const auto law=[](int step) {return unsigned(std::floor(255.0*std::cos(step*3.14159265358979323846/60.0)+1e-9));};
        const auto gain=[&](unsigned weight) {const auto tenths=volume*weight*10/255;return tenths/10+(tenths%10>=5?1u:0u);};
        return {gain(law(pan+15)),gain(law(15-pan))};
    }

    // RPC 0x409. A movie that ends is stopped first (logged); a skipped movie destroys its player while it
    // plays. The announced data is then reported back as a stop reports it, without being heard: the
    // game's decoder gets its buffer space back only through those reports, and the sources go on to
    // the next film (seen as the film after a skipped one never starting; the reports themselves are
    // not captured from the original).
    void playerDestroyed(std::uint32_t handle) {
        const auto& p=known(players_,handle,"CRI destroy of an unknown player");
        if(p.playing)for(auto& [source,s]:sources_)if(s.buffer==p.buffers[0] || s.buffer==p.buffers[1]) {
            s.due=0;
            for(const auto& stretch:s.written)s.flush+=stretch.bytes;
            if(on_silenced)on_silenced(source);
        }
        players_.erase(handle);destroyed_.insert(handle);
    }

    // Stream command 0x100: `bytes` of PCM written at `address`, received on stream channel `channel`.
    void dataWritten(std::uint32_t source,std::uint32_t address,std::uint32_t bytes,unsigned channel) {
        auto& s=known(sources_,source,"CRI audio data for an unknown source");
        if(!bytes)throw std::runtime_error("CRI audio data command without data");
        s.written.push_back({address,bytes});s.channel=channel;announced_bytes+=bytes;
    }
    // Stream command 2: start (argument 1) or stop (argument 0) the player. A stop ends playback and the
    // next returned buffer reports all data still announced as played (logged at the end of the logo movie).
    void play(std::uint32_t player,std::uint32_t argument) {
        if(argument>1)throw std::runtime_error("CRI player command 2 with argument "+std::to_string(argument)+" is not modelled");
        // A start or stop still queued for a player destroyed meanwhile (a skipped film) has nothing to act on.
        if(!players_.count(player) && (!argument || destroyed_.count(player))) {++commands_for_destroyed_players;return;}
        auto& p=known(players_,player,"CRI play command for an unknown player");
        if(argument && !p.playing) {p.playing=true;p.clock=initial_phase;}
        if(!argument) {
            p.playing=false;
            for(auto& [source,s]:sources_)if(s.buffer==p.buffers[0] || s.buffer==p.buffers[1])
                for(const auto& stretch:s.written)s.flush+=stretch.bytes;
        }
    }
    // One field elapses: playing players advance and their sources consume the blocks that ended.
    void field() {
        for(auto& [handle,p]:players_) {
            if(!p.playing)continue;
            p.clock+=field_ticks;const unsigned blocks=unsigned(p.clock/block_ticks);p.clock%=block_ticks;
            for(auto& [source,s]:sources_)if(s.buffer==p.buffers[0] || s.buffer==p.buffers[1])s.due+=blocks*p.block;
            played_blocks+=blocks;
        }
    }
    // Messages for the buffer returned on `channel`: the data played since the last call, as stretches
    // of the announced writes (a stretch ends where the written addresses stop being contiguous).
    std::vector<Message> played(unsigned channel) {
        std::vector<Message> out;
        for(auto& [handle,s]:sources_) {
            if(s.channel!=channel)continue;
            // Played blocks first (heard), then what a stop reports without playing it.
            while((s.due || s.flush) && !s.written.empty()) {
                auto& front=s.written.front();auto& budget=s.due?s.due:s.flush;
                const auto take=front.bytes<budget?front.bytes:budget;
                if(!out.empty() && out.back().ee_object==s.ee_object && out.back().address+out.back().bytes==front.address)out.back().bytes+=take;
                else out.push_back({s.ee_object,front.address,take});
                if(&budget==&s.due && on_audible)for(const auto& [player,p]:players_)for(unsigned side=0;side<2;++side)
                    if(p.buffers[side]==s.buffer) {
                        const auto g=gains(p.volume,p.pan[side]);const auto hertz=std::uint32_t(std::uint64_t(p.block)*48000/block_bytes);
                        const std::array<int,4> kind{int(p.channels),int(p.volume),p.pan[side],int(hertz)};
                        if(mixes.size()<256 || mixes.count(kind))mixes[kind]+=take;
                        on_audible({handle,g[0],g[1],p.channels==1,hertz,front.address,take});
                    }
                front.address+=take;front.bytes-=take;budget-=take;
                if(!front.bytes)s.written.pop_front();
            }
            // Blocks that ended without announced data play nothing (not seen in the logs, where data
            // always precedes the start).
            s.due=s.flush=0;
        }
        return out;
    }

    static constexpr std::uint32_t block_bytes=0x400;

private:
    // Clock in fifths of a sample: a field is 800.8 samples (48,000 Hz at 59.94 fields/s), a block 512.
    static constexpr std::uint64_t field_ticks=4004,block_ticks=2560,initial_phase=1440; // Phase as captured.
    struct Stretch {std::uint32_t address=0,bytes=0;};
    // last_written: one more than the field of the last data written (0: none yet).
    struct Source {std::uint32_t buffer=0,ee_object=0,due=0,flush=0;unsigned channel=0;std::deque<Stretch> written;};
    struct Player {
        std::array<std::uint32_t,2> buffers{};bool playing=false;std::uint64_t clock=0;std::uint32_t block=block_bytes;
        unsigned volume=255,channels=2;std::array<int,2> pan{};
    };
    std::map<std::uint32_t,Source> sources_;
    std::map<std::uint32_t,Player> players_;
    std::set<std::uint32_t> destroyed_; // Handles of destroyed players, until one is created again.
    // A player that exists, or null for one destroyed meanwhile (its queued command is dropped and counted).
    Player* live(std::uint32_t player,const char* error) {
        if(!players_.count(player) && destroyed_.count(player)) {++commands_for_destroyed_players;return nullptr;}
        return &known(players_,player,error);
    }
    template<class Map> static auto& known(Map& map,std::uint32_t handle,const char* error) {
        const auto found=map.find(handle);
        if(found==map.end())throw std::runtime_error(std::string(error)+" "+std::to_string(handle));
        return found->second;
    }
};
