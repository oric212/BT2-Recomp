// Quick start (the player's setting "Quick start", BT2_QUICK_START=1; docs/plans/launcher/LAUNCHER_PLAN.md): the game
// starts at its main menu with the save on the memory card loaded (or, on a card without one, a save file created as
// the title's New Game creates it), without showing the logos, the opening film or the title. Nothing of the game is skipped: its own code runs the whole way, and the port presses the buttons a player
// would, where the game itself accepts them:
// - The card stage runs as always (the game checks the card and loads the save before the logos); shown, so a question
//   the game asks there (no save, a damaged card) reaches the player.
// - From the first logo (the logo player 0x3BC000) Start is pressed: the game skips the logos it lets a player skip
//   (Spike's and the middleware's it does not), the opening film, the copyright and "Press START button".
// - On the title's menu (0x3B9710 with the menu shown), Cross only while the title stands in its menu:
//   - with a save loaded (three entries, the progress flags holding 0x8 or 0x800) on Continue, the third entry, where
//     the game puts the cursor; the game then goes to the main menu with the loaded save;
//   - without one (two entries) on New Game; the game asks "Do you wish to create a save file on the memory card?"
//     (title state 0x12, No preselected): Left to Yes, then Cross; the game creates the save file ("Save successful",
//     shown) and goes to the main menu. Any other question the game asks stops quick start.
// - The main menu (0x3B4250) ends it.
// Until then the run goes in fast forward, silent, and from the first logo the pictures are covered. A button the player
// presses, a stage that takes too long, or game code that is not the expected code ends it at once and hands the game
// to the player wherever it is. Tool runs use it too (the checks: tools/ps2recomp/quick_start_check.py).
#pragma once
#include <cstdint>
#include <functional>
#include <string>

class EeRuntime;

namespace quick_start {
struct Hooks {
    std::function<void(bool)> fast;  // Fast forward on or off (FramePacer::starting_run); may be empty.
    std::function<void(bool)> cover; // The game's pictures covered or shown (ViewerWindow::setCovered); may be empty.
};
// Installs the hooks and the presses (EeRuntime::pad_presses). The overlay's code is checked when it first runs.
void install(EeRuntime& runtime,Hooks hooks);
// Whether quick start is still under way.
bool running();
// One line: how it ended (the main menu reached, at which field, the progress flags), or why it stopped.
std::string summary();
}
