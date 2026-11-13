# Current progress

*Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441) as a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). How each part was checked: [VALIDATION.md](progress/VALIDATION.md). Dated detail: [November](progress/2026-11.md), [October](progress/2026-10.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg?v=f80871b5)

| Milestone | Status |
| --- | --- |
| Startup | ✅ Verified — boot to the frame loop; key states match captures of the original |
| Controller and card checks | ✅ Verified — native services; the no-card prompt matches the original |
| Films | 🟢 Operational — logo and opening films with sound |
| Title and menus | 🟢 Operational — every main-menu mode; all Ultimate Battle Z courses, tournaments and training lessons run without input |
| Versus battle | 🟢 Operational — playable with a controller; all 130 fighters and 16 stages sampled; the fight's logic and draw steps and the AI match PCSX2 exactly at every frame compared |
| Story mode | 🟢 Operational — all 200 parts and 73 towns run without input; partly played by hand |
| Audio | 🟢 Operational — music, voices and effects; confirmed by listening |
| Saves | ✅ Verified — loading, saving and overwriting; the saves byte-identical to the original's in PCSX2 |
| Enhancements | 🟢 Operational — HD textures, super-sampling to 16x, widescreen, 60 fps battles, fast loading, beam struggles by buttons; full speed with all of them |
| Online battle | 🟢 Operational — lockstep over UDP with connection codes, no port to open; played between two homes; each player sees their own view, radar and prompts; rounds start locked on |

**Code:** 11,139 EE functions plus 494 in the `DBZP.BIN` overlay, all compiled natively; nothing interpreted in the gate's runs · **Gate:** unit tests, Python suites, 31/31 comparisons with the original, the battle run against its baseline.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Latest

- **2026-11-13 — v0.1 released** ([release](https://github.com/oric212/BT2-Recomp/releases/tag/v0.1)): the first release package with a short player's guide (install, play, known issues) and release notes.
- **2026-11-13 — the game's service calls against PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), step 8): every SIF RPC request and synchronous reply from boot to the title demo, with each buffer's first 24 words. Three faults in the port's native replies fixed (LOADFILE's module IDs, CRI's handles, FILEIO's first file descriptor); memory card, disc control, controller, IOP heap, LOADFILE, FILEIO and CRI now equal PCSX2's, SOUNDS but for two words that follow the IOP's layout. The disc reads are the same sectors in other chunks (timing); the remaining request differences are registers and IOP bytes the game does not read. Open: the controller ports' asynchronous replies.
- **2026-11-12 to 13 — the fight against PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), steps 1 to 9): the port runs the game's code from PCSX2's own state, compared byte for byte. The fight frame's logic and draw steps match at every frame compared (300 consecutive frames, a full versus battle to its K.O.); VU1's memory after a frame's chain, every texture and palette word of GS memory and the loaded fighters and stage equal; the pictures within the two renderers' rounding. Four faults found and fixed on the way (`sqrt.s`'s register and rounding, `movn`/`movz`'s width, VU1's I-bit pairs). The game's own input delay equal; press to screen 104 ms in the port with Low input lag, 108 ms in PCSX2.
- **2026-11-13 — Quick start** ([launcher plan](plans/launcher/LAUNCHER_PLAN.md), "Quick start"): straight to the main menu with the card's save loaded (a save created when the card has none); 7.1 s to the main menu.
- **2026-11-12 — after the user's online match:** leaving an online battle no longer leaves the other game stuck; fighters' faces stay HD; under water on Namek keeps its super-sampled detail. Also: the Namek lava fix at 60 fps, HD packs in nested folders, the launcher as Play / Online / Settings.
- **2026-11-11:** online rounds start locked on; "Low input lag" (a picture shown when drawn, 18.7 ms sooner in a 30 fps fight); fighters' HD textures no longer pop.
- Earlier: [November](progress/2026-11.md), [October](progress/2026-10.md).

## Open reports (the user's, not yet investigated)

- **The destroyed Namek still flickers** at 60 fps after the lava fix ([60 fps plan](plans/graphics/HIGH_FRAME_RATE_PLAN.md)).
- **Frieza over the ocean with HD:** a doubled outline, broken lines, blocky water ([HD textures plan](plans/graphics/HD_TEXTURES_PLAN.md)).
- **Characters under water** drawn without HD textures (same plan); the low resolution under water is fixed (2026-11-12).
- **"Press O to recover" not always shown online** (the user, 2026-11-12): no port fault found ([online plan](plans/online/ONLINE_BATTLE_PLAN.md)); which fighters, attack and side?
- A texture glitch in the first HD fight (not reproduced headless).

## Waiting for the user's play

Online on two PCs with the release package (lock-on from the start, the connect screen, pausing); Low input lag; on the RX 9070 XT the HD faces and the fade; Variable refresh rate, beam struggles by buttons, the second controller.

## Measurements

One PC (Core i9-10900K, Radeon RX 9070 XT), Release, a fight with every enhancement ([performance findings](reference/PERFORMANCE_FINDINGS.md)).

| Item | Value |
| --- | --- |
| Fight | 29.97 flips a second, super attacks included; an in-between picture for nearly every flip, 6.3 ms each |
| Game thread | 10 to 14 ms of a 33 ms frame |
| Graphics card | 6 ms a frame plain, 8 to 10 with 16x, HD and widescreen |
| Latency | About 7 fields of the game's own from a button to the screen, plus the port's: with Low input lag a picture goes out 9 to 14 ms before its flip. Press to screen, windowed (median): port 104 ms with Low input lag (105 with HD, 16x and widescreen), 111 ms at 60 fps (117 with them), 133 ms without Low input lag; PCSX2 108 ms |
| Memory | 0.7 to 0.85 GB; 0.8 to 1.2 GB of video memory |

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Every mapped function compiled; EE floating-point rounding and register widths checked against PCSX2; threads, semaphores, interrupts |
| IOP services | 🟢 Operational | Native disc, file, controller, memory card, sound and CRI services; no IOP code runs; their replies equal PCSX2's from boot to the title demo but for the controller ports' asynchronous ones and module-local words ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS and display | 🟢 Operational | paraLLEl-GS on its own threads, native passes for the outline, glow and depth of field |
| GIF / VIF / VU | 🟢 Operational | DMA as the game uses it; VU1 programs native, checked in the gate |
| IPU / video | 🟢 Operational | Both films decode through the game's Sofdec library |
| Audio | 🟢 Operational | ADX streams and a native effects sampler, 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Operational | Keyboard, XInput and DualSense; mapping in the launcher and the in-game menu |
| Memory card | ✅ Verified | A host folder per card; saves byte-identical to the original's ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Verified | ISO reads; archive table and overlay byte-identical to the original |

## Next

In the [implementation plan](project/IMPLEMENTATION_PLAN.md)'s order:

1. **The PCSX2 comparison** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md)): the controller ports' asynchronous replies; the rest of loaded data (the globals by route); other screens' logic (step 4); VU1 and GS memory on other screens; the comparisons into the gate (step 10).
2. **The open reports** above, as the user gives detail.
3. **Picture and speed:** an edge filter, 60 fps load work, responsiveness steps 7 and 8 (waiting for the user's go).
4. **Later:** textures the HD pack lacks upscaled by the port ([plan](plans/graphics/TEXTURE_UPSCALING_PLAN.md)); the exhaustive battle sweep.
5. **The user's decisions:** a matchmaking server and rollback for online; publishing the release.

**Not compared with the original yet:** other screens' logic, loaded data at other events than a fight's first frame, VU1 memory on other screens, GS memory and pictures on other screens, the services after the title demo.

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md)
