# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). What has been checked, and how: [VALIDATION.md](progress/VALIDATION.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Startup | ✅ Verified — boot, modules answered natively, frame loop; key states match captures of the original |
| Controller and card checks | ✅ Verified — native services; the no-card prompt matches the original |
| Films | 🟢 Operational — logo and opening films with sound, also after a skip |
| Title and menus | 🟢 Operational — title, New Game and all ten main-menu screens |
| Versus battle | 🟢 Operational — 1P vs COM with a controller; CPU vs CPU for characters 0 to 129 on all 16 stages (sampled) |
| Story mode | 🟢 Operational — without input: all 200 playable parts to their battle's end, and all 73 towns behind optional markers (conversations, items, Dragon Balls, the shop, fights); not yet played through by the user |
| Audio | 🟢 Operational — music, voices and effects; confirmed by the user's listening |
| Saves | 🔧 Incomplete — the user's save loads; saving not yet tried in the game |

**Code:** 11,139 EE functions plus 494 in the `DBZP.BIN` overlay, all compiled natively; the gate's runs and the story sweep interpret nothing · **Gate:** unit tests, Python suites, 32/32 comparisons with the original, the battle run against its baseline.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements

| Item | Value |
| --- | --- |
| Functions entered | 2,007 / 1,998 in the gate's Debug run (1,000 frames, compared with the original); 3,732 in the gate's Release battle run (5,100 frames, against its own baseline); 3,500 to 3,800 per CPU-vs-CPU battle without input |
| Release run time | The scripted fight (5,100 frames, headless) about 14.5 s; one battle without input to 4,500 flips about 70 s. One scene on one PC, not a gameplay performance figure |
| Largest cost | The GPU renderer's packet intake: 3.7 s of the scripted fight ([performance plan](plans/ee/PERFORMANCE_PLAN.md)) |

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Every mapped function compiled, with its interior entries; unmapped code runs through a logged interpreter bridge and is compiled at the next build; threads, semaphores, interrupts, VBlank waits; five generator errors corrected when sources are prepared (`SQRT.S`, VF0 writes, VU0 random numbers, `MFC1` sign extension, `VCLIP` flags) |
| IOP services | 🟢 Operational | Native disc, file, heap, LOADFILE, controller, memory card, sound and CRI services; no IOP code runs ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS | 🟢 Operational | paraLLEl-GS draws every screen reached, 2D and 3D, and answers readbacks; the CPU GS model draws 2D and is the test reference |
| GIF / VIF / VU | 🟢 Operational | GIF and VIF1 DMA as the game uses them; VU0 macro mode inline; VU1 microprograms interpreted or native (the heaviest hand-written), checked against the interpreter in the gate |
| IPU / video | 🟢 Operational | MPEG-2 decoding through the game's Sofdec library; both films play |
| Audio | 🟢 Operational | ADX streams and a native sampler for effects, mixed to 48 kHz stereo; WAV file or audio device ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Operational | Scripted input, keyboard, XInput and DualSense; mapping in the launcher and the in-game menu |
| Memory card | 🔧 Incomplete | A host folder per card; loading confirmed by the user; saving not yet tried ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Verified | ISO reads; archive table and overlay byte-identical to the original |

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Order set by the user (2026-10-13, 2026-10-18, 2026-10-19); details in the [handoff](progress/ACTIVE_PASS.md):

1. **Whole-game coverage** ([plan](plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)): story mode's remaining screens as runs show them ([plan](plans/ee/STORY_MODE_PLAN.md)); runtime special cases generalized as runs reach them; exhaustive battle sweeps at the end, as validation.
2. **The user's tests of what is built:** special moves and ultimates, saving, the launcher's screens, the camera in fights.
3. **Performance** ([plan](plans/ee/PERFORMANCE_PLAN.md)): fights at a flat 30 pictures per second and menus at 60 on a mid- to low-end PC; next the GPU renderer's intake on a graphics thread.
4. **Later:** launcher extras (HD textures), 3D and film comparisons with the original, the CPU renderer in 3D (optional).

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
