#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>

// A texture's colours in brief, to tell lit variants of one texture apart (docs/plans/graphics/HD_TEXTURES_PLAN.md,
// step 9): the colour averaged over each of 4 x 4 equal cells of the texture, weighted by alpha, and how much
// of each cell alpha covers. The game's texture with its current palette and a pack's HD image of it give
// the same summary when they hold the same colours, whatever their sizes.
struct TextureColours {
    static constexpr unsigned side=4,cells=side*side;
    std::array<std::array<float,3>,cells> rgb{}; // 0 to 255.
    std::array<float,cells> cover{};             // 0 to 1.

    // Adds `count` pixels of one colour (alpha 0 to 1) to a cell; finish() makes the sums averages.
    void add(unsigned cell,float r,float g,float b,float alpha,float count);
    void finish();
    // The root mean square difference of the colours of the cells both cover, 0 to 255; none when no
    // cell is covered in both.
    static std::optional<float> distance(const TextureColours& a,const TextureColours& b);
    // The summary of a block-compressed image level (BC1, BC2 or BC3 by `bc`; others are not read): its
    // 4 x 4 pixel blocks decoded, alpha 255 as 1. None for another format or a level smaller than 4 x 4.
    static std::optional<TextureColours> ofBlocks(unsigned bc,unsigned width,unsigned height,std::span<const std::uint8_t> blocks);

private:
    std::array<float,cells> pixels_{};
};
