// Native CRI ADX audio flow (docs/plans/video/SOFDEC_PLAN.md): what the IOP's player reports back
// while a stream plays. From a PCSX2 log of the audio channel (capture_ee.py --cri-stream-log,
// 2026-10-12): the EE announces PCM it wrote to IOP memory (command 0x100 {source, address, bytes}),
// and once the player is started (command 2 {player, 1}) each returned buffer carries one message per
// played stretch, {0, EE object, address, bytes}, in the order the data was written; a stop (command 2
// {player, 0}) reports the rest at once. The IOP plays
// 48 kHz 16-bit data in 512-sample blocks (0x400 bytes per source); a field (800.8 samples) reports
// the blocks that ended in it. No sound is produced here; time is logical (one field per VBLANK).
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class CriAudioFlow {
public:
    struct Message {std::uint32_t ee_object=0,address=0,bytes=0;};
    unsigned played_blocks=0,announced_bytes=0;

    // RPC 0x400 created a source (one audio channel) on an IOP buffer object for an EE object; RPC 0x408
    // a player over two buffer objects.
    void sourceCreated(std::uint32_t handle,std::uint32_t buffer,std::uint32_t ee_object) {sources_[handle].buffer=buffer;sources_[handle].ee_object=ee_object;}
    void playerCreated(std::uint32_t handle,std::uint32_t first,std::uint32_t second) {players_[handle]={{first,second},false,0};}

    void playerDestroyed(std::uint32_t handle) {
        if(known(players_,handle,"CRI destroy of an unknown player").playing)throw std::runtime_error("CRI destroy of a playing player");
        players_.erase(handle);
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
        auto& p=known(players_,player,"CRI play command for an unknown player");
        if(argument && !p.playing) {p.playing=true;p.clock=initial_phase;}
        if(!argument) {
            p.playing=false;
            for(auto& [source,s]:sources_)if(s.buffer==p.buffers[0] || s.buffer==p.buffers[1])
                for(const auto& stretch:s.written)s.due+=stretch.bytes;
        }
    }
    // One field elapses: playing players advance and their sources consume the blocks that ended.
    void field() {
        for(auto& [handle,p]:players_) {
            if(!p.playing)continue;
            p.clock+=field_ticks;const unsigned blocks=unsigned(p.clock/block_ticks);p.clock%=block_ticks;
            for(auto& [source,s]:sources_)if(s.buffer==p.buffers[0] || s.buffer==p.buffers[1])s.due+=blocks*block_bytes;
            played_blocks+=blocks;
        }
    }
    // Messages for the buffer returned on `channel`: the data played since the last call, as stretches
    // of the announced writes (a stretch ends where the written addresses stop being contiguous).
    std::vector<Message> played(unsigned channel) {
        std::vector<Message> out;
        for(auto& [handle,s]:sources_) {
            if(s.channel!=channel)continue;
            while(s.due && !s.written.empty()) {
                auto& front=s.written.front();
                const auto take=front.bytes<s.due?front.bytes:s.due;
                if(!out.empty() && out.back().ee_object==s.ee_object && out.back().address+out.back().bytes==front.address)out.back().bytes+=take;
                else out.push_back({s.ee_object,front.address,take});
                front.address+=take;front.bytes-=take;s.due-=take;
                if(!front.bytes)s.written.pop_front();
            }
            // Blocks that ended without announced data play nothing (not seen in the logs, where data
            // always precedes the start).
            s.due=0;
        }
        return out;
    }

    static constexpr std::uint32_t block_bytes=0x400;

private:
    // Clock in fifths of a sample: a field is 800.8 samples (48,000 Hz at 59.94 fields/s), a block 512.
    static constexpr std::uint64_t field_ticks=4004,block_ticks=2560,initial_phase=1440; // Phase as captured.
    struct Stretch {std::uint32_t address=0,bytes=0;};
    struct Source {std::uint32_t buffer=0,ee_object=0,due=0;unsigned channel=0;std::deque<Stretch> written;};
    struct Player {std::array<std::uint32_t,2> buffers{};bool playing=false;std::uint64_t clock=0;};
    std::map<std::uint32_t,Source> sources_;
    std::map<std::uint32_t,Player> players_;
    template<class Map> static auto& known(Map& map,std::uint32_t handle,const char* error) {
        const auto found=map.find(handle);
        if(found==map.end())throw std::runtime_error(std::string(error)+" "+std::to_string(handle));
        return found->second;
    }
};
