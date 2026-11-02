// Beam struggles by buttons (docs/plans/input/BEAM_STRUGGLE_PLAN.md), the player's setting: in a beam
// struggle a player-controlled fighter pushes by pressing Square (X on an Xbox pad) as well as by turning the sticks.
// The struggle measures each fighter's push once a frame with one function (0x217E58): the angle either
// stick turned since the previous frame, counted below 90 degrees, the larger of the two, in radians. The
// generated function still runs; for a fighter the player controls, its result is raised to what the
// presses give. CPU fighters and the sticks are unchanged.
#pragma once
#include <cstdint>

class EeRuntime;

namespace struggle_buttons {
// What one new press of Square is worth, in radians of stick turn (about 0.38 of a turn: steady
// mashing at 7 presses a second matches turning a stick about 2.7 times a second).
constexpr float press_turn=2.4f;
// The most a frame gives, as the original's largest result (just under a quarter turn).
constexpr float frame_turn=1.5f;

// One fighter's frame: the pressed buttons now and in the previous frame (active-high, PadScript bit order),
// what its sticks measured, and the turn the presses have not paid out yet (carried from frame to frame).
// Returns the frame's push; `owed` is updated.
float push(std::uint32_t held,std::uint32_t previous,float sticks,float& owed);

// Wraps the struggle's push measure when its code is the expected one; returns whether it did.
bool install(EeRuntime& runtime);
}
