# GPU compatibility

What a graphics card other than the RX 6800 XT (RDNA 2) the port was tuned on showed, how it was found, and what changed. The renderer is paraLLEl-GS with the project's patches (`config/parallel_gs.lock.json`).

## RX 9070 XT (RDNA 4), 2026-11-09: the fade into a fight

**The user's report:** after changing to an RX 9070 XT, the first moments of every fight showed vertical colour stripes over the whole picture and a large, blocky, see-through copy of a fighter; the rest of the fight was fine. It showed with every setting off.

**How it was found** (each step on the RX 9070 XT, Release, the CPU fight `BT2_START_BATTLE=0,1,0`):

- The user switched settings off one by one (60 fps, light depth of field, HD, 16x, widescreen, variable refresh) and drew the window with GDI instead of the GPU presenter (`BT2_VIEWER_GDI=1`): still there. So it was in the pictures.
- Every picture saved from flip 250 to 600 (`BT2_GPU_GS_SNAPSHOT`), scored by the mean difference between neighbouring columns: flips 510 onward five times the rest. The pictures: the intro's last close-up striped, with its colours rotated, then fading over the fight. Headless runs give the same pictures.
- The GS stream (`BT2_GIF_DUMP`, `gif_dump_draws.py`): when the intro ends, BT2 reads its framebuffer back as 24-bit data (`local->host`, PSMCT24, 512x224 twice), keeps it, uploads it again as 24-bit (`host->local`) and draws it over the fight's first frames, fading.
- The bytes handed back to the game (`BT2_READBACK_DUMP`): the first readbacks clean, the one after the first re-upload already striped. GS memory after the upload (`BT2_GS_PEEK`): striped. So the upload's unpacking was wrong, not the readback.
- paraLLEl-GS unpacks large uploads on the GPU (`vram_copy.comp`): the packed source is read as `u8vec3` at a 3-byte stride (scalar layout, through a buffer device address), the destination written as `u8vec3` into 4-byte words. Rewritten to read and write single bytes: the stripes went. The control: the unchanged shader compiled the same way still striped. Byte-identical pictures otherwise (12 fight and 6 story pictures; the only differences were the intro's earlier cuts, faintly striped before, clean after).

**The fix:** `tools/graphics/gpu/parallel_gs_ct24_upload.patch`. `slangmosh.hpp` (normally made by Granite's slangmosh, which this machine does not have) takes the shader compiled with the Vulkan SDK's `glslc --target-env=vulkan1.1` and the include path of `slangmosh.json`, as an array of its own; the descriptor layout is unchanged. To rebuild it after another change to `vram_copy.comp`: compile it so and replace the array.

**Also found:** the synchronization validation (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `VK_VALIDATION_VALIDATE_SYNC=true`) reported a write after read: the HD textures' palette copy out of GS memory (`GpuGsBackend::copyDrawnPalettes`) ended with a barrier for the host only, so a following upload into GS memory could overwrite the pages it was still reading. Its barrier now orders later copies too; the validation reports nothing in a fight with HD.

**Open:** with HD textures some fighters' faces and muscle lines are missing (Goku's at flip 1200 of the CPU fight, HD at 1x or 16x; the same without HD is right). Not explained by the palette hazard (the same picture after its fix); not yet known whether it is the card, the pack or the lit-variant choice.
