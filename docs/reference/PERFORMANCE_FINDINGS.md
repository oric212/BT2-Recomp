# Performance and latency findings

What is known about the port's speed and its delay between the game and the screen, as of 2026-11-02: the measured state, where the time goes, what was changed and what was tried and left out, how to measure it without fooling oneself, and what is left. The requirements and the history of each step are in the plans this summarises: the [performance plan](../plans/ee/PERFORMANCE_PLAN.md), the [responsiveness plan](../plans/ee/RESPONSIVENESS_PLAN.md), the [native fight rendering plan](../plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md) and the [hybrid rendering plan](../plans/graphics/HYBRID_RENDERING_PLAN.md); the day-by-day record is in the [November history](../progress/2026-11.md).

All figures are from one PC (Core i9-10900K, Radeon RX 6800 XT), Release builds. "The user's settings" means fullscreen at window scale 4 (the 4x scanout), 16x super-sampling, HD textures, widescreen, the HD outline, Light depth of field and 60 fps battles.

## The state on 2026-11-02

| | Value | Measured on |
| --- | --- | --- |
| Fight rate | 30 pictures a second, the original's, with every enhancement; super attacks 29.96 to 29.97 at 16x with 60 fps | window and fullscreen runs |
| 60 fps fights | 98% of in-between pictures shown (4,690 to 4,692 of 4,782), 2 to 4 not ready in time | story fight, user's settings, fullscreen, 5,000 flips |
| An in-between picture | 11.0 to 12.4 ms to draw (13.5 that morning, 18 to 19 the day before) | the same |
| Flip to screen, menus and 30 fps fights | 10 ms median in fullscreen, 17 ms in a window | the gate's path at 1x |
| Flip to screen, 60 fps fights | 11.3 ms median in fullscreen (24.5 before 2026-11-03's pacer and in-between timing); with VRR each picture a field after the last within 1 ms (97%) | story fight, user's settings |
| The game's thread | about 0.7 ms of a field's 16.7 on the gate's path; 10 to 14 ms per 33 ms flip in a 60 fps fight with HD | headless and fullscreen |
| Graphics card | about 7 ms a picture at 16x with every enhancement, 3 ms at 4x | headless, `BT2_GPU_TIMESTAMPS` |
| Hitches | four fields at screen changes take 22 to 47 ms (loading work landing in one field); none in fights | the gate's path, headless |
| Loading | "Fast loading" (off by default) saves 7.1 of 109 s on the gate's path | window, paced |

## Where the time goes

**Threads.** The game's thread runs the recompiled game, VIF1 and VU1 and checks the GS data; the main renderer's GS thread names textures and feeds paraLLEl-GS; with 60 fps an in-between thread blends the fight's VIF1 lists and runs VU1 again, and a second renderer has its own GS thread; a presenter thread shows the pictures.

**The 60 fps critical path** is the in-between picture: its thread pairs and blends the lists (about 1.2 to 1.6 ms), runs VIF1 and VU1 (2.9 to 3.3 ms) and then waits about 7 ms for the second renderer's GS thread, which works 8.9 ms a frame. That GS thread's time is now mostly paraLLEl-GS's own intake (its packed-vertex parsing, vertex kicks and triangle append, about 7% of its samples) and a few tenths of a percent per item of our own code (texture names, the fighters pass, the command sink).

**Why 60 fps fights reach the screen later than 30 fps ones.** The game flips about 0.9 ms into its field and the flip's picture is ready 0.1 ms later. The in-between picture can only start after the flip and is finished 11 to 12 ms later, around the next field boundary; it is handed over after the game's frame work in its field (8.7 ms in, median). Both pictures must go out at the same moment in their fields to stay a field apart, so that moment sits late in the field, and the flip's picture waits about 15.6 ms before it is handed to the window. Only a faster in-between picture can move it.

**The game's thread** (60 fps fight, HD, 16x) spends its working time in the recompiled code with the runtime's per-call dispatch and per-access checks, the hand-written VU1 programs (already SIMD code per VU1 instruction pair, spread evenly, no hot line), VIF1, the GS packet checks, the EE FPU (3.6% since its change) and copies of DMA data.

**The graphics card** is not the limit on this PC. At 16x about 2.5 ms of a 7 ms picture is the depth of field's and bloom's full-screen layers, which paraLLEl-GS shades for all 16 samples; they cannot be shaded once per pixel because their alpha carries per-sample data (see below).

## What was changed on 2026-11-02

Every change kept the output exact: pictures byte-identical (main and in-between), the guest state per field identical (`BT2_STATE_HASH`), the texture naming log identical, the gate's VU1 hash unchanged, or a unit test against the old behaviour; each passed the gate.

| Change | Gain | Where |
| --- | --- | --- |
| The blend finds the copies of the cameras' matrices in one walk of the list instead of one per camera and half | blending 1.69 to 1.23 ms a picture | `vu/vif1_blend.cpp` |
| VU1's packet hash only for the gate (`BT2_VU1_KICK_HASH`), never in the in-between renderer | the game's thread 0.22 ms a flip; in-between thread 4.25 to 3.55 ms of processor time a picture (with the blend change) | `vu/vu1.cpp` |
| Texture names: 32-bit uploads copied a block row's pixel pairs at a time, 256-entry palettes read in place | main GS thread about 0.7 ms a flip, second about 0.45 ms a frame | `gs/textures/gs_texture_names.cpp` |
| EE FPU: rounding toward zero from the round-to-nearest result and its exact error (Fast2Sum, FMA where the processor has it, else the old path) instead of loading MXCSR twice per operation | the FPU's share of the game's thread 7.2% to 3.6% | `ee/ps2_fpu.cpp`, `tests/fpu_tests.cpp` |
| The in-between recording takes the gathered VIF1 list's buffer instead of copying 2.4 MB per list | about 3% of the game thread's work | `ee/ps2_runtime_memory.cpp`, `gs/gpu/in_between_frames.cpp` |
| The fighters pass keeps its buffers per thread; the names' vertex runs call kick at most twice | second GS thread 0.25 to 0.4 ms a frame | `gs/gpu/native/fighters.cpp` |
| The second renderer replays the game's readbacks without waiting for the GPU (paraLLEl-GS's `unsynced_readbacks` on that renderer only) | 24 waits of about 59 ms each over 2,000 flips gone; that GS thread 9.54 to 8.92 ms a frame | `gs/gpu/gpu_gs_backend.cpp` |
| 60 fps fights show both pictures ahead of their deadlines at the same moment in their fields | flip to screen 26.5 to 27.3 ms to 24.5 ms, the same pictures shown | `probe/entry_probe.cpp` |
| Fast loading as a setting (silent loading stretches run free) | 7.1 s on the gate's path when on | `host/loading_skip.h` |

## Tried and left out

| Idea | Why not |
| --- | --- |
| Handing the in-between picture over at its field's start | often not finished then: 4,604 instead of 4,692 shown, flip to screen 25.2 ms, no gain |
| The command sink decoding ST and colour once per vertex run | no gain beyond the noise of a busy PC |
| Blending the list in place instead of copying it | the game's thread now runs VIF1 from the recorded list at the same time |
| The full-screen effect layers shaded once per pixel (hybrid plan step 4b) | up to 1.2 ms of GPU saved but the depth of field and outline broke: their per-sample alpha is data |
| The fighters' triangles into paraLLEl-GS in bulk | 0.05 ms |
| The stage drawn natively | 0.83 ms of GPU against more processor work |
| Palette loads that change nothing kept from moving the names' generation | no measurable load |
| Rewriting the hand-written VU1 programs further | already SIMD per instruction pair; no hot line, exactness risk |
| Spreading the hitches at screen changes over fields | would change the game's timing; only faster code helps |

## Measuring without fooling oneself

- **Headless and paced runs measure different things.** An unpaced headless run is GPU-bound at 16x: the processor queues many frames ahead, and anything that waits for the GPU waits for all of them. The game's readbacks in the CPU fight's opening took 60 to 160 ms each headless but 11 to 16 ms paced in fullscreen. Compare processor work headless; judge waits, latency and shown pictures paced (fullscreen at the user's settings).
- **Noise.** Identical runs vary by about 0.2 to 0.3 ms per frame on a GS thread when the PC is quiet and by 1 ms or more while other programs run (a browser, chat, the editor). Repeat each side at least twice; alternate old and new builds; a gain smaller than the spread is not a gain.
- **Evenness numbers vary between runs:** the share of presents a field apart went from 84 to 94 to 82% across three runs of the same build.
- **Compare against the committed build in the same conditions:** stash, rebuild, run, restore. A profile is only readable with the map of the build that took it.
- **Exactness first:** byte-compare pictures (`BT2_GPU_GS_SNAPSHOT`, `BT2_SIXTY_FPS_SNAPSHOT`, check mode), the state hash per field and the texture naming log; HD runs are not repeatable in their in-between pictures (replacements stream in with timing), so exactness checks run without HD.
- **Per-call-site timing** finds stalls a sampling profile shows only as "waiting": temporary timers around paraLLEl-GS's `wait_timeline` call sites found the readback waits in one run.

The tools: `BT2_PROFILE_TOTALS`, `BT2_SAMPLE_PROFILE` and `BT2_SAMPLE_GS_THREADS` with `tools/perf/profile_report.py` and `profile_lines.py`, `BT2_GPU_TIMESTAMPS`, `BT2_FIELD_LOG`, `BT2_FLIP_TIMES`, `BT2_LATENCY_LOG` with `tools/perf/latency_report.py`; the 60 fps line at the run's end (per in-between picture: blend, VIF1/VU1, waits, processor time; the second GS thread per frame). See [TOOLS.md](../guides/TOOLS.md).

## What is left, by expected gain

1. **60 fps latency (24.5 ms):** an in-between picture finished a few milliseconds sooner after the flip. Its thread waits mostly on the second renderer's GS thread, whose remaining time is paraLLEl-GS's intake: a patch there (a faster packed-vertex path, fewer per-primitive checks) is the only large lever left, and earlier bulk-intake work saved little.
2. **Presentation:** variable refresh rate or presents allowed to tear (about 3 to 4 ms; they change the look, so they would be settings the user chooses).
3. **Hitches at screen changes (22 to 47 ms):** faster per-call dispatch and memory checks in the recompiled code (about a third of those fields), and the film-style picture decoding (22% of them) with SIMD.
4. **GPU load:** lower super-sampling (4x or 8x) is the practical lever; the per-sample effect layers are the game's data.
5. **Small items** (0.1 ms each): the in-between thread's unpack walk, the palette copies from drawn pages, the sink's per-vertex work.
