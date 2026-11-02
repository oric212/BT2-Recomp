# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). What has been checked, and how: [VALIDATION.md](progress/VALIDATION.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Startup | ✅ Verified — boot, modules answered natively, frame loop; key states match captures of the original |
| Controller and card checks | ✅ Verified — native services; the no-card prompt matches the original |
| Films | 🟢 Operational — logo and opening films with sound, also after a skip |
| Title and menus | 🟢 Operational — title, New Game and all ten main-menu screens; all 15 Ultimate Battle Z courses, Dragon Tournaments and the 21 Ultimate Training lessons run without input |
| Versus battle | 🟢 Operational — 1P vs COM with a controller; CPU vs CPU for characters 0 to 129 on all 16 stages (sampled) |
| Story mode | 🟢 Operational — all 200 parts to their battle's end and all 73 towns without input; not yet played through by the user |
| Audio | 🟢 Operational — music, voices and effects, balanced in both ears; confirmed by the user's listening (the left-only speech fix of 2026-10-20 not yet) |
| Saves | 🔧 Incomplete — the user's save loads; saving not yet tried in the game |
| Enhancements | 🟢 Operational — HD textures (PCSX2 names), super-sampling, widescreen and 60 fps battles in the launcher; 60 fps flicker and glitches fixed 2026-10-25 (not yet seen by the user); HD and 60 fps together at 16x since 2026-10-26 (30 flips per second, 83 to 91% of the in-between pictures shown); HD textures streamed, and the fighters' relit bodies drawn with the pack's nearest lit variant (2026-10-26); the replacements' full detail at a 4x scanout and the game's effects at the super-sampled resolution (2026-10-26, not yet seen by the user); the ink outline at the super-sampled resolution as a native pass inside paraLLEl-GS, on by default with the look the user chose and a toggle in the menus (2026-10-27; exact at 1x); the super attack's glow as a native pass, shaded once per pixel with super-sampling (2026-10-27: 16.4 to 10.0 ms of the GPU's shading at the beam's peak; the super attack at 29.97 flips per second in the window with every enhancement and 60 fps, was 29.04); with 60 fps the fighters drawn by the GPU's rasterizer (2026-11-02); the flashing line across the ground at 16x fixed and a light depth of field as a setting (2026-11-02, pictures checked by the user, not yet in play) |

**Code:** 11,139 EE functions plus 494 in the `DBZP.BIN` overlay, all compiled natively; the gate's runs and the story sweep interpret nothing · **Gate:** unit tests, Python suites, 31/31 comparisons with the original, the battle run against its baseline.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements

One PC (Core i9-10900K, Radeon RX 6800 XT), Release build, a paced versus fight in the window ([performance plan](plans/ee/PERFORMANCE_PLAN.md)).

| Item | Value |
| --- | --- |
| Fight | 30 pictures per second (the original's rate), also with every enhancement; the game's thread works 12 to 24 ms of 33 with HD textures, 60 fps and 16x (the GS work on GS threads since 2026-10-25); a super attack at 16x with 60 fps 29.97 per second (the glow as a native pass since 2026-10-27, was 29.0) |
| Processor and memory | The game's thread, a GS thread, a present thread and with 60 fps two more; 0.7 GB (0.85 GB with every enhancement) |
| Graphics card | 6 ms per frame plain, 10 ms with 16x super-sampling, HD textures and widescreen; 0.8 to 1.2 GB of video memory |
| Responsiveness | The game's thread 0.7 ms of a field's 16.7 (median, the gate's path; 27% less since 2026-10-27); flip to screen 10 ms in fullscreen and 17 ms in a window since pictures go out ahead of their deadline (2026-10-27; 20 and 27 before); a 60 fps fight 24.5 ms in fullscreen at every enhancement since 2026-11-02 (49 with the field it was shown later before, 26.5 with its pictures at the deadlines); three hitches at screen changes, 26 to 50 ms ([responsiveness plan](plans/ee/RESPONSIVENESS_PLAN.md)) |
| Functions entered | 3,732 in the gate's battle run (5,100 frames); 3,500 to 3,800 per CPU-vs-CPU battle |

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Every mapped function compiled; unmapped code through a logged interpreter bridge, compiled at the next build; threads, semaphores, interrupts |
| IOP services | 🟢 Operational | Native disc, file, heap, controller, memory card, sound and CRI services; no IOP code runs ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS and display | 🟢 Operational | paraLLEl-GS draws every screen, the only renderer; the picture goes to the window on the GPU |
| GIF / VIF / VU | 🟢 Operational | DMA as the game uses it; VU1 programs native (the heaviest hand-written), checked in the gate |
| IPU / video | 🟢 Operational | Both films decode through the game's Sofdec library |
| Audio | 🟢 Operational | ADX streams and a native effects sampler, 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Operational | Keyboard, XInput and DualSense; mapping in the launcher and the in-game menu |
| Memory card | 🔧 Incomplete | A host folder per card; loading confirmed; saving untried ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Verified | ISO reads; archive table and overlay byte-identical to the original |

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Details in the [handoff](progress/ACTIVE_PASS.md) and the [implementation plan](project/IMPLEMENTATION_PLAN.md):

0. **The user's requests of 2026-11-02:** beam struggles by Square/Cross with a matching prompt, a setting on by default ([plan](plans/input/BEAM_STRUGGLE_PLAN.md); waiting for the user's recorded struggle); the first fight's texture glitch (HD readers built, not reproduced headless). Fast loading is among the Enhancements and on by default.
1. **Super attacks at 16x:** 29.96 to 29.97 flips per second with every enhancement and 60 fps in every super attack measured (ten fights swept, three measured in the window) since the glow pass (2026-10-27); left: the user's look in play ([hybrid rendering plan](plans/graphics/HYBRID_RENDERING_PLAN.md) step 3).
2. **60 fps:** revised by the user toward [native fight rendering](plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md) (2026-11-01; step 1, the fighters by the GPU's rasterizer, built 2026-11-02; step 2 revised by the user the same day: the fighters' vertices from VU1's output without per-vertex work on the GS thread, begun with vertex runs decoded once and a fighter piece's two layers in one draw, 14.6 to about 12.4 ms per in-between picture headless; since then load work: the in-between thread's processor time 4.25 to 3.55 ms a picture headless at 4x by the last change). Measured at the user's settings in fullscreen on a story fight: 98% of in-between pictures shown, flat 29.97, 14.7 ms of processor time each. The flicker the user saw, a line across the ground at 16x from the depth of field, fixed 2026-11-02.
3. **Blur** the user sees in some text and screens and on the fighters' edges, then **the randomizer** (the random fight music checked correct and varying with timing; the user's path needed, [handoff](progress/ACTIVE_PASS.md)); then the user's look at the HD textures (finished 2026-10-26, [HD textures](plans/graphics/HD_TEXTURES_PLAN.md) step 10) and the fighters' lit variants.
4. **Picture:** FSR 1 scaling with sharpening, an edge filter ([image quality](plans/graphics/IMAGE_QUALITY_PLAN.md)).
5. **The user's tests:** saving, the enhancements in play, the sound fix, fast forward, the play test fixes of 2026-10-21, 60 fps battles in play.
6. **Final validation:** the exhaustive battle sweep. The runtime generalization plan's groups 4 to 6 are done (2026-10-27; the census 108 to 35 lines, two items left with their reason in the [plan](plans/ee/RUNTIME_GENERALIZATION_PLAN.md)); the battle run's baseline is now `7f996c1524a22bea`.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [November 2026 history](progress/2026-11.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [performance findings](reference/PERFORMANCE_FINDINGS.md) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
