#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Vulkan {class Device;}

// An HD texture pack made for PCSX2's texture replacement (docs/plans/graphics/HD_TEXTURES_PLAN.md):
// the replacements by name, loaded as GPU images on first use within a video memory budget.
// - Index: every .dds below the pack's `replacements` folder (the folder itself if it has none), by
//   name as PCSX2 reads it (the format word's bit 14 cleared). A name found twice keeps the first in
//   path order (counted). With PlayStation buttons, the pack's `Original Buttons` folder (pack 2)
//   replaces the button art of `replacements` (Xbox art there).
// - Images: DDS in BC1, BC2, BC3 or BC7 with their mip levels, kept compressed and read as UNORM (the
//   game's colours are not sRGB-encoded). Other files are refused by name (counted, drawn as originals).
// - Budget: when the loaded images exceed it, the least recently used are dropped (the renderer may
//   still hold one until the game overwrites its texture).
// find may be called from two threads (a second renderer on the device, gs/gpu/in_between_frames.h);
// loadPending runs only while the other thread draws nothing.
class HdTexturePack {
public:
    struct Image;
    HdTexturePack(Vulkan::Device& device,const std::filesystem::path& folder,std::uint64_t budget_bytes,bool playstation_buttons=true);
    ~HdTexturePack();
    // The replacement for a texture name ("<texture>-<palette>-<format>" or "<texture>-<format>"); null
    // when the pack has none or it is not loaded yet: a replacement asked for is loaded by the next
    // loadPending, so the renderer never creates images while it takes GS data. A name with a palette
    // part finds, in turn, the pack's name itself, its name without a palette part (every palette, as in
    // PCSX2), and another palette's variant of the texture (the first in path order; the user's choice of
    // 2026-10-21): the game relights the fighters' palettes every frame and a pack holds a few of them, so
    // exact names alone would leave the bodies original most of the time. A variant holds the lighting it
    // was captured with, so the renderer draws a variant as detail over the game's own colours (exact:
    // false; tools/graphics/gpu/parallel_gs_texture_replacement.patch) and the lighting stays the game's.
    // A texture the pack holds with several palettes always gets its one variant as detail, also when a
    // name matches exactly: switching between the two as the lighting changes would flicker.
    // Images are kept by file, so all palettes of a texture share one.
    // `exact` (optional): the file is made for this palette (or every palette), not another palette's variant.
    const Image* find(const std::string& name,bool* exact=nullptr);
    // Loads the replacements asked for since the last call (at a frame boundary, after the renderer's
    // flush) and keeps the budget.
    void loadPending();

    std::size_t names() const {return index_.size();}
    unsigned duplicates=0,refused=0,loaded=0,evicted=0,found=0,missing=0,waiting=0;
    std::uint64_t resident_bytes=0,peak_bytes=0;

    // A pack's file name as PCSX2 reads it (exposed for tests).
    static std::string normalized(const std::string& stem);
    struct Dds {std::uint32_t width=0,height=0,format=0,block_bytes=0;std::vector<std::vector<std::uint8_t>> levels;};
    // A DDS file's block-compressed levels; none for a file this does not read (exposed for tests).
    static std::optional<Dds> readDds(const std::vector<std::uint8_t>& file);

private:
    Vulkan::Device& device_;std::uint64_t budget_;std::uint64_t tick_=0;
    std::unordered_map<std::string,std::filesystem::path> index_;
    struct Entry;
    std::unordered_map<std::string,Entry*> cache_;
    std::vector<std::string> pending_;
    std::unordered_map<std::string,std::filesystem::path> variants_; // "<texture>-<format>": one of its palettes' file.
    std::unordered_set<std::string> relit_; // Textures the pack holds with more than one palette.
    std::mutex lock_;
    void evict();
};
