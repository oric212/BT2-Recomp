// In-between frames for 60 fps battles: the GS packets the game builds itself (VIF1 DIRECT blocks: effects
// such as a fighter's aura or dust, the sky, the radar, already projected to the screen) set in between two
// frames, so that they stay with the blended fighters and camera (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md,
// the user's report of 2026-11-03: an aura left where the newer frame has it covered a body drawn halfway).
// The blocks of the two lists are aligned by their opening GIF tag. In a pair that differs, each GIF tag that
// writes vertices is a unit, known by its tag and the PRIM and TEX0 it draws with; a unit of the newer list takes
// the nearest unused unit of the older one with the same key and vertex count (an aura's flames come and go and
// change order, so the place in the block is no identity), when every vertex moved at most `max_pixels` and all
// alike (within a quarter of that around the first vertex's move: a polygon the game clips anew each frame, as
// the sky's, has its corners on other edges). Units that share a vertex position anywhere in the list (the sky's
// polygons, each in a block of its own) are blended only all together. Paired vertices get their screen position
// and depth blended at t; colours, texture coordinates and fog stay the newer frame's, as does everything else;
// at t = 1 the result is the newer list exactly.
#pragma once
#include "vu/vif1_blend.h"
#include <cstdint>
#include <span>
#include <vector>

// Blends `out` (the newer list, or a copy of it) at t against `older`; returns the vertices blended. The lists'
// DIRECT blocks (walkVif1) when known, else found here.
std::size_t blendGsPackets(std::vector<std::uint32_t>& out,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                           const std::vector<Vif1Unpack>* older_blocks=nullptr,const std::vector<Vif1Unpack>* newer_blocks=nullptr,
                           float max_pixels=128.0f);
