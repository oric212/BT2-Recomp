# HD textures

Requested by the user on 2026-10-20 as the more important of two enhancements (the other is [60 fps battles](HIGH_FRAME_RATE_PLAN.md)): load HD replacement textures, made for PCSX2, in place of the game's own. Supersedes the "HD textures" section of the [launcher plan](../launcher/LAUNCHER_PLAN.md), which stays the place for the setting.

## The packs (investigated 2026-10-20)

Two local packs, `work/HD_TEXTURES` (never committed; their threads are credited in the README when the feature ships, as the credits files there ask):

| | Pack 1 | Pack 2 |
| --- | --- | --- |
| Files | 1,244 PNG, 191 MB | 15,097 DDS, 8.3 GB |
| Made for | the "DX Mod" of SLUS-21441 (a modified game, per its thread's title) | the retail SLUS-21441 |
| Layout | one `replacements` folder | `replacements` sorted into Characters (bodies 5.6 GB, fight, select and turn pictures, posters), Stages (768), Others (998), Text (266), Tutorial (186), Buttons (50) |
| Format | PNG, mostly 64 to 1024 pixels a side | BC3 (DXGI 77) with full mip chains, mostly 256 to 1024, up to 2048 |
| Names shared with the other pack | 0 | 0 |

**Pack 2 is the one to use** (the user's preference, and the evidence agrees): it is made for the disc we run, covers characters, stages, text and the tutorial instead of a selection, and is already block-compressed with mips, which the GPU samples directly. Pack 1's textures come from a modified game, so their hashes are not expected to match retail data at all; the name check (step 1) confirms or refutes this with numbers.

Pack 2 details that need a decision:

- Its `replacements/Buttons` are the Xbox button art (byte-identical to its `Xbox Layout` folder); `Original Buttons` holds the same 47 names with PlayStation art. Offer it as a setting (PlayStation by default).
- 159 names appear in two subfolders. Which file wins must be fixed (and reported), not left to directory order.
- Almost all textures are 4-bit (12,743) or 8-bit (2,245) paletted; 12 names have no palette part.

## How a texture gets its name

PCSX2's texture replacement (pinned `GSTextureReplacements.cpp` and `GSTextureCache.cpp`, commit `81526d4d`) is the behavioural reference. The behaviour to reproduce, in our own code:

- File name `<texture hash>-<palette hash>-<format word>` in hexadecimal (`<texture hash>-<format word>` without a palette; `-r<w>x<h>` variants for clamped regions, which pack 2 does not use).
- **Texture hash:** XXH3-64 over the texture's GS memory as the GS stores it: the 256-byte blocks of the TW × TH rectangle in block order. A texture smaller than one block, or one whose format does not fill all 32 bits of a pixel (24-bit, the 8H/4HL/4HH formats), is hashed after reading it out to rows instead (indices for paletted formats, 32-bit pixels otherwise), so neighbouring data in the same blocks does not change the name. Mip levels are hashed into the same state only when mipmapping is on.
- **Palette hash:** XXH3-64 over the palette as the GS has loaded it, expanded to 32 bits per entry (16 entries for 4-bit, 256 for 8-bit, from CSA on).
- **Format word:** PSM (6 bits), TW and TH (4 bits each), one unused bit, and for non-paletted formats with alpha expansion TEXA's TA0 (8), AEM (1) and TA1 (8).

XXH3 is a public hash with a BSD-licensed reference implementation (a single-header library); it would be a new external dependency.

## How to draw with them

The GPU renderer is paraLLEl-GS (snapshot `cc6184af`). It keeps a cache of textures: when a draw needs a texture not in the cache, `create_cached_texture` makes an RGBA image of the texture's rectangle and a compute shader fills it from GS memory, palette applied. The ubershader samples these images with normalised coordinates (texel coordinates times the reciprocal of the texture's size), with clamping and region modes in texel units of the original size. So an image with more pixels, holding the same picture, is sampled correctly by the same coordinates. That is the hook:

1. When a texture is created, compute its PCSX2 name; if the pack has it, bind the HD image instead of uploading. Its size stays the original's in the texture's info, so coordinates and clamps are unchanged.
2. Where paraLLEl-GS caches only a sub-rectangle of the texture (clamp regions), the HD image covers the full TW × TH: the offset and scale go into the texture info.
3. Its exact bilinear path (fetching neighbouring texels with a phase table, for clamped textures) steps one original texel; with an HD image it should use the hardware's linear filter instead.

paraLLEl-GS is a vendored snapshot; the change is carried as a patch in the repository (LGPL-3.0 allows it), kept as small as possible.

**The names need GS memory on the CPU.** paraLLEl-GS holds GS memory on the GPU. Reading it back for each new texture would stall the GPU. The CPU GS model already writes image transfers into its own copy of GS memory; it stopped doing so in GPU-only runs for speed ([performance plan](../ee/PERFORMANCE_PLAN.md), "Done"). With HD textures on, it does so again, and hashes from that copy. Textures the game renders itself (render targets read as textures) are not in the copy, and PCSX2 does not replace those either.

**Loading:** 8.3 GB cannot be resident. An index of names is built at start (15,000 file names, no reads); a texture is loaded on its first use on a worker thread (the game draws the original until it is ready, as PCSX2 does), into GPU memory with a size budget and least-recently-used eviction.

## What it will look like

paraLLEl-GS does super-sampling (2× to 16×), not free upscaling. With 4× or more and `high_resolution_scanout`, it presents at twice the resolution (1,280 × 896) and samples textures at up to four times the density per axis, so HD textures show real detail there. That is the ceiling: output at 1440p or 4K the way PCSX2's hardware renderer upscales is not something paraLLEl-GS does. How far that can be pushed, and widescreen, are the [image quality plan](IMAGE_QUALITY_PLAN.md). Its notes also warn that a game which copies its frame to the display buffer through a texture loses the super-sampled detail; whether BT2 does is checked in step 4.

## Order of work

1. **Name check (decides everything after it).** A diagnostic in tool runs: for each texture the GPU renderer creates, compute the PCSX2 name and report how many are found in each pack, per screen (title, menus, a battle, story). Expect most of pack 2's matching textures to be found on the screens that use them, and next to none of pack 1's. Compare a few names by hand with PCSX2's own dump of the same screen if the counts are low.
2. **XXH3 and the hasher** as a runtime unit with tests (block order per format, small-texture and partial-format paths, palette expansion, format word), checked against names in pack 2.
3. **The paraLLEl-GS hook** (above) with BC3 images and their mips; the button-art choice; the duplicate rule.
4. **Super-sampling setting** (off, 4×, 8×, 16×) and high-resolution output, measured in a battle in Release (GPU and processor time per picture).
5. **The user's inspection:** pictures of the same scenes with and without the pack in `work/frames/`.
6. **Settings:** the launcher's and the overlay's "HD textures" entry becomes available, with the pack folder `data/textures` next to the programs (like the disc image) and the button art choice.
