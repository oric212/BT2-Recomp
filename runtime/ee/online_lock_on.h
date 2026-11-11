// Online battles (docs/plans/online/ONLINE_BATTLE_PLAN.md): each round starts with both fighters locked on (the user,
// 2026-11-11: at a match's start both players had to look for each other). Lock-on is bit 0x1 of a fighter's flag word 0
// (fighter +0x2B0): the HUD shows "Lock on!" when it is newly set (0x111B50, message kind 2, through 0x20BE80), and supers
// need it. The game sets it (0x20DBC0) only while the fighting phase runs and the opponent is within range (the sum of
// the two fighters' ranges, command object +0xFAC: 72.4 in the measured fight), and clears it only on certain events (a
// held release, two fighter states), not by distance. The fighters start a round 176 apart, so in 1P vs CPU the CPU
// flies in and both lock on, and in 1P vs 2P nobody is locked on until the players come close. Online, the first frame of
// a round's fighting phase (bit 0x400000 of that word, which the fighter's frame update 0x1F71B0 sets from the battle's
// phase, 0x105A48, before calling 0x2115D0) sets the lock as the game's own setter (0x2113C0) does. The same on both
// machines, so the lockstep stays alike; after that the game's own rules apply. A deliberate change from the original's
// two-player fights, online only.
// Installed only when the guest code hashes as expected, as the native functions are (ee/native_functions.h).
#pragma once
#include <cstdint>

class EeRuntime;

namespace online_lock_on {
constexpr std::uint32_t locked_on=0x1,fighting=0x400000;
// A fighter's flag word 0 at its frame update, and whether its previous update was in the fighting phase (updated):
// the word with the lock set on the first fighting frame, unchanged otherwise.
std::uint32_t atUpdate(std::uint32_t flags,bool& was_fighting);
}

// Returns whether the fighters' frame update was hooked.
bool registerOnlineLockOn(EeRuntime& runtime);
