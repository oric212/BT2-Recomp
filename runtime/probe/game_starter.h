#pragma once
#include "ee/ee_runtime.h"
#include <cstdint>
#include <string_view>

// Tool runs only (WHOLE_GAME_COVERAGE_PLAN.md, step 6): game states started without input. The
// overlay's entry, which runs the title and the menus, is replaced on its first call. The replacement
// does the part of the entry's first-run work later screens and battles need (mounts the second archive
// and loads the common files; no card check, logos or films), then:
// - BT2_START_BATTLE: sets a CPU-against-CPU battle up with the setter calls the title's demo fight
//   makes (characters, variants, stage, music; CPU on both sides) and the versus settings, and returns,
//   so main runs its battle routine;
// - BT2_START_SCREEN: writes the mode the main menu would and runs the original entry, which shows
//   that screen.
// Later calls run the original entry.
struct BattleStart {
    std::uint32_t character[2]{};
    std::uint32_t variant[2]{};
    std::uint32_t stage = 0;
    std::uint32_t music = 9; // The music the versus menus chose with Wasteland.
};

// "<character>[:<variant>],<character>[:<variant>],<stage>[,<music>]", decimal; throws on other text.
BattleStart parseBattleStart(std::string_view text);
void installBattleStarter(EeRuntime& runtime, const BattleStart& start);

// A mode of the overlay's mode table, 1 to 64, decimal; throws on other text.
std::uint32_t parseScreenStart(std::string_view text);
void installScreenStarter(EeRuntime& runtime, std::uint32_t mode);
