# Image quality and widescreen

Requested by the user on 2026-10-20 with the [HD textures](HD_TEXTURES_PLAN.md) and [60 fps](HIGH_FRAME_RATE_PLAN.md) plans: the best picture the port can give, and widescreen.

## Where detail comes from

Three separate steps decide how sharp the picture is:

1. **Rendering resolution:** how many pixels the GS work is computed at. The game draws at 640 × 448. This is the only step that adds detail; HD textures pay off only as far as it goes.
2. **Texture detail:** the textures sampled while rendering ([HD textures](HD_TEXTURES_PLAN.md)).
3. **Display scaling:** how the finished picture is enlarged to the window. Integer scaling (whole multiples, each pixel a sharp block) or smooth scaling (bilinear, or FSR1 with sharpening as paraLLEl-GS's author suggests) changes how the pixels look, but adds no detail. Integer scaling is already a display setting ([launcher plan](../launcher/LAUNCHER_PLAN.md), Overlay).

**What paraLLEl-GS gives.** Super-sampling at 2×, 4×, 8× or 16×, and with 4× or more a scanout at twice the resolution (1,280 × 896) built from the samples. 16× is 4 × 4 samples per game pixel. The scanout at 2× uses them as 2 × 2 output pixels with 2 × 2 samples each (anti-aliased). A scanout at 4× (2,560 × 1,792, one sample per pixel, no anti-aliasing) would be the most detail the samples hold; paraLLEl-GS does not offer it, and whether its scanout can be extended that way is to be found out (step 3). Its notes warn that a game which copies its frame to the display buffer through a texture loses the extra detail; BT2 has to be checked for that.

**Integer scaling with these:** 1,280 × 896 fits a 1080p screen once (with borders) and a 4K screen twice (2,560 × 1,792). Widescreen stretches the picture horizontally by 4/3, which is not a whole multiple: there, integer scaling can apply vertically only, or smooth scaling is used.

## Widescreen

PCSX2's patch database has a 16:9 patch for BT2's PAL release (SLES-54164, by sergx12, ported to PAL by ElHecht), none for SLUS-21441. It changes the projection: four floats of 432.0 (`0x43D88000`) to about 576 (cut-scenes, gameplay, versus 1P and 2P), the instructions that load 0.75 or 1.0 next to them, and one for the sky. Our disc has the same kind of table: 432.0 at `0x386D94` to `0x386E2C`, and `lui at, 0x3F40` near `0x12BA08` (the PAL sky fix is at `0x12BD38`). So the patch's behaviour can be found in our executable and applied as a setting, with the authors credited. The window then shows the picture at 16:9. Open questions: whether the HUD and menus stretch (the patch may leave 2D at 4:3 positions), and whether objects are culled at the new screen edges.

## Order of work

1. **Widescreen:** match each of the PAL patch's changes to SLUS-21441's code by its surroundings, make it a setting that writes them at the right moment, the window at 16:9; pictures of a battle, a cut-scene, the menus and the story map for the user.
2. **Super-sampling and high-resolution output** as settings (with [HD textures](HD_TEXTURES_PLAN.md) step 4), measured in Release. **Switch built 2026-10-20:** `BT2_SUPER_SAMPLING=<1|2|4|8|16>`; 4 or more scan out at 1,280 × 896. The Ultimate Battle Z screens and a fight with pack 2 (2,000 flips, Release): 27 s at 4×, 31 s at 16×, no renderer errors; the picture at 1,280 × 896 with smooth edges, the select screen's portraits crisp at twice the size (`work/frames/hd-textures/zoom-*.png`: original, HD at 1×, HD at 16×). Left: the setting for players and the frame time in a fight at each rate.
3. **A 4× scanout** from 16× samples: find out whether paraLLEl-GS's scanout can be extended; decide from what it costs.
4. **Display scaling:** integer (both axes for 4:3, vertical only for 16:9), bilinear, FSR1 with sharpening; the user compares pictures.
