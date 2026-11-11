// Short presses kept for the game (the user, 2026-11-11: mashing Triangle in a fight, some presses did nothing).
// The game's controller state is the host's at each field; in a fight the game reads it every other field, so a tap that
// begins and ends between two of its reads never reaches it. The host's controls are sampled between fields as well
// (HostInput's sampler); a press seen there, or at a field, is given to the game for at least two fields, so a fight's
// read takes it. Presses longer than that are passed as they are: their release is not delayed (Square acts on its release).
#pragma once
#include <array>
#include <cstdint>

class PressLatch {
public:
    // `now`: the buttons held at this field; `seen`: those held at any moment since the last field (sampled between
    // fields). Returns the buttons the game gets for this field.
    std::uint16_t field(std::uint16_t now,std::uint16_t seen) {
        const std::uint16_t risen=std::uint16_t((now|seen)&~last_); // Pressed since the last field.
        std::uint16_t out=now;
        for(unsigned b=0;b<16;++b) {
            if(risen>>b&1)left_[b]=2;
            if(left_[b]) {out|=std::uint16_t(1u<<b);--left_[b];}
        }
        last_=now;
        return out;
    }

private:
    std::uint16_t last_=0;
    std::array<std::uint8_t,16> left_{}; // Fields a new press is still given for.
};
