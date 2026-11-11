// "Low input lag" (docs/plans/ee/RESPONSIVENESS_PLAN.md, step 6): each picture shown as soon as the game has drawn
// it instead of at the flip that displays it.
//
// The game flips a picture and at once draws the next into the other buffer; that picture is complete by the end of
// the flip's field and displayed at the next flip, a frame later (a field in menus, two in fights). Measured on the
// gate's path and a story fight, every flip: the other buffer at the end of a flip's field, scanned out with the
// display registers of the flip before, equals the next flip's picture, and the GS receives nothing between that
// field's end and the next flip. So the next picture goes out at the end of the flip's field (before the field's
// deadline is waited for), and the next flip, when it shows what was predicted, presents nothing.
//
// The rule checks both conditions every time: a picture is shown early only while the display alternates between
// two sets of registers (A, B, A), and a flip whose registers differ from the prediction, or that comes after GS input
// arrived since the early picture, presents its own picture as before. The game's state is untouched: only when its
// pictures reach the screen changes.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

class EarlyPicture {
public:
    using Registers=std::array<std::uint64_t,15>; // The GS privileged registers at a flip (EeRuntime::gs_privileged).

    // A flip: its registers and the GS input so far (a running count). `allowed`: whether the picture after this
    // flip may be shown early (not in fast forward, not where 60 fps battles show in-between pictures).
    // Returns whether this flip's picture was already shown early, unchanged: the flip then presents nothing.
    bool flipped(const Registers& registers,std::uint64_t gs_input,bool allowed) {
        const bool shown=pending_ && registers==predicted_ && gs_input==input_at_show_;
        pending_=false;
        history_[0]=history_[1];history_[1]=history_[2];history_[2]=registers;
        if(known_<3)++known_;
        flip_in_field_=allowed;
        if(shown)++shown_early;else ++shown_at_flip;
        return shown;
    }
    // The end of a field, before its deadline is waited for. When a flip came in this field and the display
    // alternates, the registers that will display the next picture: show it now with them.
    std::optional<Registers> fieldEnded(std::uint64_t gs_input) {
        if(!flip_in_field_)return std::nullopt;
        flip_in_field_=false;
        const bool alternating=known_==3 && history_[2]!=history_[1] && history_[2]==history_[0];
        if(!alternating)return std::nullopt;
        pending_=true;predicted_=history_[1];input_at_show_=gs_input;
        return predicted_;
    }
    // The early picture could not be shown after all (no scanout): the next flip presents its own.
    void cancel() {pending_=false;}

    unsigned shown_early=0,shown_at_flip=0; // Statistics: flips whose picture went out early, and the others.

private:
    std::array<Registers,3> history_{}; // The last three flips' registers, oldest first.
    unsigned known_=0;
    bool flip_in_field_=false,pending_=false;
    Registers predicted_{};std::uint64_t input_at_show_=0;
};
