// In-between frames for 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): the VIF1
// list of a frame with the transforms that move between two frames set in between. Two successive fight
// frames send mostly the same codes in the same order (7,000 to 8,000 unpacks a frame); a few pieces of
// geometry change size or come and go, so the unpacks of the two lists are aligned like lines in a diff
// (Myers' algorithm), each unpack known by its code (address, count, format).
// What is blended (read from the lists on 2026-10-25; as Tenkaichi3Decomp blends only the model and screen
// matrices and never what would jump):
// - Cameras: a camera block (12 quadwords whose last four, the viewport, are the same in both frames) has
//   its screen matrices (the first eight quadwords, projective) blended linearly, and every copy of them in
//   the newer list (each object carries the camera's matrices with its own data) becomes the blended one,
//   so the fighters and the stage share the in-between camera.
// - Bones: rigid transforms (three axes of sensible length at nearly right angles with w 0, then a position
//   with w 1) are blended where the older frame has one at the same place, the axes turning (their lengths
//   blended) and the position moving, unless an axis turned by more than 45 degrees or the position jumped.
//   The game streams its data in fixed-size V4-32 chunks, so a chunk's data can shift between frames when
//   something before it changed size: the older data around the pair (the unpacks before, at and after it)
//   is searched for the shift that lines the transforms up, when no shift does not.
// Everything else is the newer frame's; at t = 1 the result is the newer list exactly. A camera whose view
// axes turn by more than 25 degrees between the two frames is a cut: the lists are not blended (no
// in-between picture across it).
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

struct Vif1Unpack {std::size_t start=0,words=0;unsigned format=0;std::uint32_t code=0;}; // format: the UNPACK code's bits 0 to 3 (VN, VL).

// The data of each UNPACK of a list, in order; nullopt when the list cannot be walked (a code this does
// not size, or data past the end).
std::optional<std::vector<Vif1Unpack>> vif1Unpacks(std::span<const std::uint32_t> words);

// Two lists' unpacks (vif1Unpacks) and their aligned pairs (index pairs into them), found once for
// cameraTurn and blendVif1; nullopt when the two differ by more than `max_edits` unpacks (not the same scene).
struct Vif1Pairing {const std::vector<Vif1Unpack>* older=nullptr;const std::vector<Vif1Unpack>* newer=nullptr;std::vector<std::pair<std::size_t,std::size_t>> pairs;};
std::optional<Vif1Pairing> pairVif1(const std::vector<Vif1Unpack>& older,const std::vector<Vif1Unpack>& newer,std::size_t max_edits=1024);

// `newer` with its transforms blended at t (above). `blended` counts the words changed, `unmatched`
// the newer list's unpacks without a partner, `refused` the changed pairs with transforms none of which was
// blended, `realigned` the pairs whose older data was found shifted.
std::vector<std::uint32_t> blendVif1(const Vif1Pairing& pairing,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                     std::size_t* blended=nullptr,std::size_t* unmatched=nullptr,std::size_t* refused=nullptr,std::size_t* realigned=nullptr);
// The same from the lists alone; nullopt when a list cannot be walked or the two do not pair.
std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                                    std::size_t* blended=nullptr,std::size_t* unmatched=nullptr,std::size_t max_edits=1024,
                                                    std::size_t* refused=nullptr,std::size_t* realigned=nullptr);

// The smallest cosine between a camera view axis of `older` and the same axis of `newer` over their
// aligned camera blocks (1 when there is none, or when the lists do not pair): below cos 25 degrees the
// camera cut.
float cameraTurn(const Vif1Pairing& pairing,std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer);
float cameraTurn(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,std::size_t max_edits=1024);

// The aligned pairs of two sequences: index pairs (i, j) with a[i] == b[j], in order, of a shortest
// edit script (Myers); nullopt when it needs more than `max_edits` insertions and deletions.
std::optional<std::vector<std::pair<std::size_t,std::size_t>>> alignSequences(std::span<const std::uint64_t> a,std::span<const std::uint64_t> b,std::size_t max_edits);
