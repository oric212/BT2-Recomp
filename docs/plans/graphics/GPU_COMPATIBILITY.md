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

**The cause: AMD's driver, not the shader** (the same day, the user's request to be sure):

- *The shader follows the rules.* Disassembled, both the shader upstream ships and the same source compiled here load the `u8vec3` with `Aligned 1`; `spirv-val` accepts both with scalar block layout. paraLLEl-GS refuses to start unless `scalarBlockLayout`, `storageBuffer8BitAccess` and `bufferDeviceAddress` are enabled, and the upload's buffer starts on the 16 bytes `buffer_reference_align` declares (Granite's staging blocks are aligned to at least 16). The Khronos validation layer reports no error while the original shader runs. Its GPU-assisted validation could not run this renderer (an internal error, then a lost device).
- *A unit test settles it* (`ct24Upload` in `runtime/tests/gpu_gs_tests.cpp`): a 512 x 224 24-bit upload at block 0x1c00 over words with top bytes of their own, every word compared with the GS's rule (the three colour bytes written, the top byte kept). On the RX 9070 XT with driver 26.10.2 (LLPC), each shader loaded in turn through a temporary hook:

  | `vram_copy` | Wrong words of 114,688 |
  |---|---|
  | upstream's shipped SPIR-V | 86,016 |
  | the original source, compiled here | 86,016 |
  | only the write byte by byte | 86,016 |
  | only the read byte by byte | 0 |
  | both byte by byte | 0 |

- *What the driver does:* the wrong words hold the three bytes from the pixel's address rounded down to 4 (pixel 1 at byte 3 holds bytes 0 to 2, pixel 2 at byte 6 bytes 4 to 6, pixel 3 at byte 9 bytes 8 to 10): the load declared with 1-byte alignment is carried out as a 4-byte-aligned one, so 3 of every 4 pixels (86,016 = 3/4 of the words) are wrong. The write (`u8vec3` at a 4-byte stride, aligned) is right.
- *The driver's own compiled code* (the user's question whether the card could be faulty; `tools/graphics/gpu/driver_check/check_unaligned_load.py`): a standalone Vulkan program runs only this read over the same 512 x 224 pixels and asks the driver for the machine code it made (`VK_KHR_pipeline_executable_properties`, `VK_AMD_shader_info`). The `u8vec3` read through a buffer device address: 86,016 wrong, every one the read from the address rounded down to 4, and only at addresses not a multiple of 4. The same read through a storage buffer descriptor: none wrong; byte by byte: none wrong. The driver compiled the failing read to a single `global_load_b32` at `source + i*3`: one 4-byte load at an address that is not 4-byte aligned, although the SPIR-V says `Aligned 1`. The two correct ones became three byte loads (`global_load_u8`, `buffer_load_u8`). A 4-byte load whose address the hardware rounds down to 4 is the hardware doing what the instruction asks; the instruction is the driver's choice. So the cause is the driver's shader compiler (or its unaligned-access setting), and the card is not faulty: the error is the same on every run, word for word.
- Not measured: other AMD generations and drivers; the user's RX 6800 XT showed no stripes with the same shader. The check above is a ready report for AMD and paraLLEl-GS's author; not sent (the user's decision). After a driver update it tells whether the patch is still needed.

**The fix:** `tools/graphics/gpu/parallel_gs_ct24_upload.patch` reads the packed pixels byte by byte; the write stays upstream's. `slangmosh.hpp` (normally made by Granite's slangmosh, which this machine does not have) takes the shader compiled with the Vulkan SDK's `glslc --target-env=vulkan1.1` and the include path of `slangmosh.json`, as an array of its own; the descriptor layout is unchanged. To rebuild it after another change to `vram_copy.comp`: compile it so and replace the array. The fight's fade with only the read changed: no stripes over flips 490 to 600 (the column score at most 4.15, the striped pictures up to 16).

**Also found:** the synchronization validation (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `VK_VALIDATION_VALIDATE_SYNC=true`) reported a write after read: the HD textures' palette copy out of GS memory (`GpuGsBackend::copyDrawnPalettes`) ended with a barrier for the host only, so a following upload into GS memory could overwrite the pages it was still reading. Its barrier now orders later copies too; the validation reports nothing in a fight with HD.

**Not the card: the missing faces with HD (found 2026-11-09).** With HD textures some fighters' faces and muscle lines were missing (Goku's at flip 1200 of the CPU fight). With the lit variants off (a temporary switch) the faces came back, so it was the variant choice ([HD textures plan](HD_TEXTURES_PLAN.md), step 9), on any card. Fixed there.
