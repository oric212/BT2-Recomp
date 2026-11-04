// The game's memory at its start, made from the player's own disc image (docs/plans/launcher/RELEASE_PLAN.md):
// the executable (SLUS_214.41) read from the ISO and its loadable segments placed in main RAM, as the console's
// loader places them. Nothing of the game comes with the release. Only the executable this port was built from is
// accepted (its hash), so another disc, region or revision is refused by name.
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The executable's file, checked; throws naming the problem.
std::vector<std::uint8_t> gameExecutableFromDisc(const std::string& disc_path);
// Main RAM (32 MB) with the executable's PT_LOAD segments placed at their addresses, the rest zero.
std::vector<std::uint8_t> guestImage(std::span<const std::uint8_t> executable);
