#pragma once
#include "ee/ee_runtime.h"
#include <cstdint>
#include <string_view>

// Tool runs only (WHOLE_GAME_COVERAGE_PLAN.md, step 6): BT2_START_BATTLE starts a CPU-against-CPU
// battle without input. The overlay's entry, which runs the title and the menus, is replaced on its
// first call: the replacement mounts the second archive and loads the common files as the entry's
// first-run work does (without the card check, the logos and the films), sets the battle up with the
// setter calls the title's demo fight makes (characters, variants, map, music; CPU on both sides) and
// the versus settings, and returns, so main runs its battle routine. Later calls run the original entry.
struct BattleStart {
    std::uint32_t character[2]{};
    std::uint32_t variant[2]{};
    std::uint32_t map = 0;
    std::uint32_t music = 1;
};

// "<character>[:<variant>],<character>[:<variant>],<map>[,<music>]", decimal; throws on other text.
BattleStart parseBattleStart(std::string_view text);
void installBattleStarter(EeRuntime& runtime, const BattleStart& start);
