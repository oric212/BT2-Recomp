// In-between frames for 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): the VIF1
// list of a frame with the values that move between two frames set in between. Two successive fight
// frames send the same codes in the same order; what differs is data: the camera's and the bones'
// matrices in V4-32 unpacks (and DIRECT data, taken from the newer frame). Floats that differ in the
// k-th unpack of each list are blended; words that are not normal floats (tags, integers) are taken
// from the newer frame as they are.
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct Vif1Unpack {std::size_t start=0,words=0;unsigned format=0;}; // format: the UNPACK code's bits 0 to 3 (VN, VL).

// The data of each UNPACK of a list, in order; nullopt when the list cannot be walked (a code this does
// not size, or data past the end).
std::optional<std::vector<Vif1Unpack>> vif1Unpacks(std::span<const std::uint32_t> words);

// `newer` with each float that differs from `older` in a V4-32 unpack set to older + t (newer - older);
// nullopt when the lists do not pair (different UNPACK counts, formats or sizes). `blended` counts the
// words changed.
std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,std::size_t* blended=nullptr);
