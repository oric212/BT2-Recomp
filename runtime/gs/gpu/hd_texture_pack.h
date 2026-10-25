#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Vulkan {class Device;}

// An HD texture pack made for PCSX2's texture replacement (docs/plans/graphics/HD_TEXTURES_PLAN.md):
// the replacements by name, loaded as GPU images within a video memory budget.
// - Index: every .dds below the pack's `replacements` folder (the folder itself if it has none), by
//   name as PCSX2 reads it (the format word's bit 14 cleared). A name found twice keeps the first in
//   path order (counted). With PlayStation buttons, the pack's `Original Buttons` folder (pack 2)
//   replaces the button art of `replacements` (Xbox art there).
// - Images: DDS in BC1, BC2, BC3 or BC7 with their mip levels, kept compressed and read as UNORM (the
//   game's colours are not sRGB-encoded). Other files are refused by name (counted, drawn as originals).
// - Streaming (step 8): a loader thread of its own reads the files and makes the GPU images (Granite
//   thread index 3), so nothing is loaded on the game's thread. A renderer that first needs one waits for
//   it a little (first_wait); one not ready by then is drawn as the original without the original being
//   kept (texture_replacement_pending), so the replacement shows from the first draw after it is loaded.
// - Budget: when the loaded images exceed it, the least recently used are dropped (the renderer may
//   still hold one until the game overwrites its texture).
// find is called by the renderers on their GS threads (gs/gpu/gpu_gs_mirror.h).
class HdTexturePack {
public:
    struct Image;
    HdTexturePack(Vulkan::Device& device,const std::filesystem::path& folder,std::uint64_t budget_bytes,bool playstation_buttons=true);
    ~HdTexturePack();
    // A texture name as numbers ("<texture>-<palette>-<format>", or "<texture>-<format>" without a
    // palette), so a lookup needs no text.
    struct Key {
        std::uint64_t texture=0,palette=0;std::uint32_t format=0;bool has_palette=false;
        bool operator==(const Key&) const=default;
    };
    // The replacement for a texture name, as PCSX2 finds it: the pack's name itself, or for a name with a
    // palette part the pack's name without one (made for every palette). The first time a file is asked
    // for, waits up to first_wait for the loader. `image` null and `loading`: the pack has it and the loader
    // is still on it. Images are kept by file, so the names that find one file share its image.
    struct Found {const Image* image=nullptr;bool loading=false;};
    Found find(const Key& name);
    // Waits until the loader has nothing left to do (tests, measurements).
    void waitLoaded();

    std::size_t names() const {return index_.size();}
    unsigned duplicates=0,refused=0,loaded=0,evicted=0,found=0,missing=0,waiting=0,late=0; // late: files not ready by first_wait.
    std::uint64_t resident_bytes=0,peak_bytes=0;

    // A pack's file name as PCSX2 reads it (the format word's bit 14 cleared), as a key; none for a name
    // that is not one (exposed for tests).
    static std::optional<Key> key(const std::string& stem);
    struct Dds {std::uint32_t width=0,height=0,format=0,block_bytes=0;std::vector<std::vector<std::uint8_t>> levels;};
    // A DDS file's block-compressed levels; none for a file this does not read (exposed for tests).
    static std::optional<Dds> readDds(const std::vector<std::uint8_t>& file);
    static constexpr unsigned loader_thread_index=3;
    static constexpr double first_wait_seconds=0.005;

private:
    Vulkan::Device& device_;std::uint64_t budget_;std::uint64_t tick_=0;
    struct KeyHash {std::size_t operator()(const Key& k) const {return std::size_t(k.texture*0x9E3779B97F4A7C15ull^k.palette*0xC2B2AE3D27D4EB4Full^(std::uint64_t(k.format)<<1|std::uint64_t(k.has_palette)));}};
    std::unordered_map<Key,std::filesystem::path,KeyHash> index_;
    struct Entry;
    std::unordered_map<const std::filesystem::path*,Entry*> cache_; // By the file in index_ (its nodes stay put).
    std::deque<const std::filesystem::path*> wanted_; // To load, in the order the renderers asked.
    std::mutex lock_;std::condition_variable changed_;bool stop_=false,busy_=false;
    Entry* entry(const std::filesystem::path* file); // Under lock_: made when missing.
    void load(Entry& entry); // On the loader thread, without lock_.
    void run();
    void evict(); // Under lock_.
    std::thread loader_; // Last: it starts in the constructor.
};
