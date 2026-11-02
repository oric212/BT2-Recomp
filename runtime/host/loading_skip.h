// Fast loading (docs/plans/ee/RESPONSIVENESS_PLAN.md step 4): silent loading waits run as fast as the machine
// allows, as fast forward does, so the game's state per field stays the same and only the wall clock is shorter.
// Asked once per field, after it: a field loads when it read the disc, asked for a file or waited at one of the
// game's load-wait idle points. A stretch starts at such a field that was silent with no button held, and goes
// on while fields stay silent with no button held and the last loading field is at most `gap` fields back (a
// load is a few requests with fades and waits between them). Sound or a button ends it at once.
#pragma once
#include <cstdint>

class LoadingSkip {
public:
    static constexpr unsigned gap=30; // Fields without a load before a stretch ends (half a second).
    struct Field {
        std::uint64_t loads=0; // Disc reads, file requests and load-wait idle points so far (a running count).
        bool silent=true;       // Nothing audible was mixed in the field.
        bool held=false;        // A button is held.
    };
    // Whether the next field runs free.
    bool update(const Field& f) {
        const bool loaded=f.loads!=loads_;
        loads_=f.loads;
        if(!f.silent || f.held) {active_=false;return false;}
        if(loaded) {since_=0;active_=true;}
        else if(active_ && ++since_>gap)active_=false;
        if(active_)++fields;
        return active_;
    }
    bool active() const {return active_;}
    unsigned fields=0; // Fields run free so far.

private:
    std::uint64_t loads_=0;unsigned since_=0;bool active_=false;
};
