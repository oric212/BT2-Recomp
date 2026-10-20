#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

// PCSX2-compatible texture names (docs/plans/graphics/HD_TEXTURES_PLAN.md): the name a texture
// replacement pack made with PCSX2 gives each texture the game draws with. Follows the GS register
// writes and image transfers of the GIF stream with its own copy of GS memory and of the CLUT, and
// names a texture the first time it is drawn after its data, palette or registers changed.
//
// A name is "<texture hash>-<palette hash>-<format word>" (no palette part without a palette), in
// lowercase hexadecimal without leading zeros except the eight-digit format word:
// - texture hash: XXH3-64 over the texture's 256-byte GS blocks in block order (rows of blocks, each
//   left to right); a texture smaller than one block, or a format that does not fill all 32 bits of a
//   pixel, is hashed as read out instead: one byte per pixel (the index) for palette formats, four
//   (RGBA, 24-bit with TEXA's alpha) otherwise, row by row;
// - palette hash: XXH3-64 over the palette as 32-bit entries (16 for 4-bit, 256 for 8-bit), as read at
//   CSA;
// - format word: PSM, TW << 6, TH << 10, and for non-palette formats below 32 bits TEXA's TA0 << 15,
//   AEM << 23, TA1 << 24.
// Behaviour adapted from PCSX2's texture replacement (pinned GSTextureCache.cpp, GSClut.cpp); GS
// memory layouts from the public GS layout as in gs_command_sink.h.
class GsTextureNames {
public:
    GsTextureNames():memory_(1024*1024) {regs_[0x1A]=1;} // PRMODECONT starts at 1: PRIM's attributes apply.
    // A GS register write as the GIF delivers it (A+D, REGLIST, decoded PACKED).
    void reg(unsigned reg,std::uint64_t value);
    // IMAGE data of the host->local transfer in progress.
    void image(std::span<const std::uint8_t> data);

    struct Name {
        std::uint64_t texture=0,palette=0;std::uint32_t format=0;bool has_palette=false;
        std::string text() const;
    };
    // The name of a texture as TEX0 and TEXA describe it, from the current memory and CLUT; none for a
    // format this does not name (counted).
    std::optional<Name> name(std::uint64_t tex0,std::uint64_t texa);

    // Names drawn with, and how many times each was named (a texture is named again after a change).
    std::map<std::string,unsigned> drawn;
    // What the copy does not follow: image transfers in formats it does not write, transfers in other
    // directions or orders, CLUT loads in formats it does not read, textures in formats it does not name.
    unsigned unsupported_uploads=0,local_copies=0,unsupported_cluts=0,unsupported_textures=0;
    std::map<unsigned,unsigned> unsupported_formats; // Draws of textures not named, by PSM.
    std::uint64_t uploaded_pixels=0;

    std::vector<std::uint32_t>& memory() {return memory_;}
    std::array<std::uint16_t,512>& clut() {return clut_;}

private:
    std::vector<std::uint32_t> memory_; // GS memory, 4 MB, by word address.
    std::array<std::uint16_t,512> clut_{}; // The CLUT buffer: 32-bit entry i as halves i and i + 256.
    std::array<std::uint64_t,0x64> regs_{};
    std::uint32_t cbp_[2]{};
    // Host->local transfer in progress.
    bool transfer_=false;unsigned psm_=0,bp_=0,bw_=0,x0_=0,y0_=0,w_=0,h_=0,x_=0,y_=0;
    std::uint32_t partial_=0;unsigned partial_bytes_=0;
    // Changes since a context's texture was last named.
    std::uint64_t generation_=0;
    std::uint64_t named_generation_[2]{~0ull,~0ull},named_tex0_[2]{},named_texa_[2]{};

    void startTransfer(std::uint64_t dir);
    void writePixel(std::uint32_t value);
    void loadClut(std::uint64_t tex0);
    void kick();
};
