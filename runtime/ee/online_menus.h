// Online battles (docs/plans/online/ONLINE_BATTLE_PLAN.md): the game's menus inside a fight offer no way back to the main
// menu (the user, 2026-11-10: an online battle is left through the online menu). The fight's pause menu ("1P PAUSE":
// Continue Battle, Look at Skill List, Return to Character Select, Return to Main Menu) is a menu description of the
// game's menu system: entries of 0x20 bytes (the first the title; an entry's id at +4), their number at +4 and the
// choices' at +0x1C, the cursor at +0x18. Its last entry, id 0x11, is Return to Main Menu; online the entries' count leaves
// it out, on both machines alike, so it is neither drawn nor reachable (the cursor wraps by that count).
// Installed only when the guest code hashes as expected, as the native functions are (ee/native_functions.h).
#pragma once

class EeRuntime;

// Returns whether the menus were hooked.
bool registerOnlineMenus(EeRuntime& runtime);
