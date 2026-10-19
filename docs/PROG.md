# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). What has been checked, and how: [VALIDATION.md](progress/VALIDATION.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading (answered natively) |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 1,000 frames in the gate run |
| Controller check | ✅ Verified — passed with the native controller service |
| Memory card check | ✅ Verified — the no-card prompt matches the original; with a card folder the game reads the save (the user's play) |
| Opening movie | 🟢 Operational — logo and opening films play with their sound, also after a skip; frames match an independent decoder at spot checks |
| Title screen | 🟢 Operational — title, option list and New Game dialogues (the user confirmed the pictures) |
| Demo fight (no input) | 🟢 Operational — the CPU-vs-CPU demo fights play and return to the title (the user confirmed the frames on 2026-10-13) |
| Menus | 🟢 Operational — all ten main-menu screens open without input; Dueling, character and stage selection respond to input (the user confirmed the pictures) |
| Versus battle | 🟢 Operational — 1P-vs-COM with a controller; CPU-vs-CPU without input for characters 0 to 129 and all 16 stages (sampled, see VALIDATION.md); music and effects |
| Story mode | 🔧 Incomplete — a part runs without input from its map through its cutscene, pre-battle screen and battle to the results (200 of 207 parts can be started; a sample of 8 finished their battles); the full sweep, cleared battles, optional markers, the shop and chases are open |
| Audio | 🟢 Operational — streams and sound effects mixed, to a WAV file or the audio device; volumes in the launcher and the in-game menu; the user confirmed a WAV of menus and a fight |
| Saves | 🔧 Incomplete — the user's 100% save loads (confirmed by the user); writing saves is unit-tested, not tried in the game |

**Coverage:** **11,139 EE functions mapped and compiled** plus 494 in the `DBZP.BIN` overlay (nothing the programs name lies outside a function) · **2,007 entered / 1,998 completed** in the gate's verified run (2026-10-13) · verification: the gate passes; 42 of 46 original comparisons pass.
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

1. **Whole-game coverage** ([plan](plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)): story mode next ([plan](plans/ee/STORY_MODE_PLAN.md): the sweep of every part's battle, then cleared battles, optional markers, the shop and chases); runtime special cases generalized as runs reach them; exhaustive battle sweeps at the end, as validation.
2. **The user's tests of what is built:** special moves and ultimates, saving, the launcher's screens, the camera in fights.
3. **Performance** ([plan](plans/ee/PERFORMANCE_PLAN.md)): fights at a flat 30 pictures per second and menus at 60 on a mid- to low-end PC; next the GPU renderer's intake on a graphics thread.
4. **Later:** launcher extras (HD textures), 3D and film comparisons with the original, the CPU renderer in 3D (optional).

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
