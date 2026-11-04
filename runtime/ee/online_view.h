// Online battles, each player full screen (docs/plans/online/ONLINE_BATTLE_PLAN.md): 1P vs 2P splits the screen,
// one half per player. The game's view setup (0x25BB38, view object and mode: 0 full screen, 1 the left half, 2
// the right) is given mode 0 for the halves too, on both machines alike, so the game culls and projects each
// player's view for the whole screen and both machines keep the same state; each view is marked by its mode in
// its scissor's top row (+0x208, 1 or 2 instead of 0). The renderer then leaves out the other player's view
// (GsCommandSink::hidden_view): the mark costs one or two lines at the top of the picture, in the overscan.
// The fights' radars, which stay where the halves had them, are left out on both machines.
// Installed only when the guest code hashes as expected, as the native functions are (ee/native_functions.h).
#pragma once
#include <cstdint>

class EeRuntime;

// Returns whether the view setup was replaced.
bool registerOnlineView(EeRuntime& runtime);
// Fight frames so far (the radars' calls): it grows while a fight runs, and stands still in the menus.
std::uint64_t onlineFightFrames();
