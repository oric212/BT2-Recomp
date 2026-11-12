# Textures the HD pack lacks, upscaled by the port

The user's request (2026-11-12): the ki blasts' and special moves' effects stay at the original's resolution with HD
textures on (no pack holds them, and an AI-upscaled set is not to be had), so the port should upscale them itself.
**A later feature:** planned now, built after the current work (the user's decision). Other ways weighed with the
user: the user upscaling `BT2_TEXTURE_DUMP`'s files with a tool of their own (works today, no code) and drawing the
effects natively (the most work, effect by effect).

## Goal

A texture the HD pack has no replacement for is drawn from an upscaled copy the port makes once, on the HD textures'
loader thread, so effects (and everything else the pack lacks) look cleaner at 4x to 16x. An enhancement:

- **Off by default,** a setting in the launcher and the in-game menu, beside HD textures; works with or without a
  pack.
- **The game unchanged:** only what the renderer samples changes; the game's state, timing and the fight are the
  same (the gate's battle hash and the 60 fps state checks).
- **No bottleneck:** nothing on the game's thread; the GS threads only look up a ready image; upscaling on the
  loader thread (or the GPU's compute queue), within the HD textures' video memory budget.

## What it plugs into

The HD textures' path does nearly all of it already ([HD textures plan](HD_TEXTURES_PLAN.md), `gs/gpu/hd_texture_pack.*`):
a texture is named as PCSX2 names it, the loader thread makes the GPU image of a replacement in the order the
renderers ask, a texture still loading is drawn from the original, and the least recently used images are dropped at
the budget. The upscaler is one more source of replacements: where the pack's lookup finds nothing, the loader makes
the image from the texture's own pixels (read from GS memory with its palette, as the name was computed) instead of
from a file. The pack keeps priority; generated images are not written to disk (a dump of them for inspection is a
diagnostic).

## The questions to settle first

1. **Which textures.** Everything the pack lacks, or only effects. Effects are not marked as such; candidates to
   measure: textures drawn with additive blending, those with soft alpha, or a list of names. Fonts, button art and
   HUD pieces the pack lacks may look worse smoothed; the first measurement lists what a fight and the menus draw
   without a replacement (`BT2_HD_SHOW_MISSING`, `BT2_TEXTURE_DUMP`).
2. **Which algorithm, by the texture's kind.** Effects are mostly soft gradients and glows: a smooth upscale
   (bicubic or Lanczos with alpha handled separately, so edges do not darken) removes the blocks and the banding but
   adds no detail. Hard-edged art (icons, outlines) suits an edge-directed scaler (xBRZ-like, as PPSSPP and Dolphin
   offer). Possibly both, chosen per texture by a cheap analysis (the share of hard edges and of flat areas).
   Real AI models are too slow while playing and stay the user's option offline.
3. **Palettes.** Most of the game's textures are 4- or 8-bit indices into a palette; indices cannot be filtered, so
   the upscaled copy is made of the colours, per texture and palette (the name already includes the palette). Effects
   that cycle or relight their palette give many such pairs: measure how many a fight makes and how fast (a cache by
   name, made once each); if too many, upscale only palettes seen for several frames, or upscale on the GPU at the
   texture's upload instead.
4. **Where it runs.** On the loader thread (the processor; effects' textures are small, 32 to 256 pixels a side) or
   as a compute pass on the GPU (faster for many, but on the renderers' device timeline). Decided by measurement:
   the time per texture and per fight's first seconds.
5. **The algorithm's source.** The project is GPL 3; an existing scaler (xBRZ is GPL 3) could be a dependency
   recorded in [LEGAL.md](../../project/LEGAL.md) with its pinned version, or written for the port from its published
   description (references are adapted, not copied).
6. **The scale.** 2x, 4x (the pack's usual), or by the super-sampling setting; the budget decides with HD on.

## Steps (when taken up)

1. **Census:** in a fight with super attacks and the menus, the textures drawn without a replacement, by format,
   size, palette changes, blending and draws (a diagnostic run, headless).
2. **Offline trial:** the census's effect textures upscaled by the candidate algorithms into a pack folder, drawn
   through the existing path, pictures of the same frames with each for the user (`work/frames/texture-upscaling/`).
   The user chooses what looks right before anything runs live.
3. **Live upscaling** in the loader for the chosen kinds and algorithm, with its setting; the census's counts and the
   loader's time per texture measured in Release at the user's settings.
4. **Checks:** tests of the scaler (sizes, alpha, palette expansion, a known image), the gate's battle hash unchanged
   with the setting on, the 60 fps in-between pictures, the budget; the user's look in play.

## Not in this plan

The effects drawn natively (a renderer feature of its own, [hybrid rendering plan](HYBRID_RENDERING_PLAN.md));
frame effects the game renders and reads back (16-bit and depth-format textures, which no pack names either).
