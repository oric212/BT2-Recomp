# Current progress

*Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441) as a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). How each part was checked: [VALIDATION.md](progress/VALIDATION.md). Dated detail: [November](progress/2026-11.md), [October](progress/2026-10.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg?v=bd120b04)

| Milestone | Status |
| --- | --- |
| Startup | ✅ Verified — boot to the frame loop; key states match captures of the original |
| Controller and card checks | ✅ Verified — native services; the no-card prompt matches the original |
| Films | 🟢 Operational — logo and opening films with sound |
| Title and menus | 🟢 Operational — every main-menu mode; all Ultimate Battle Z courses, tournaments and training lessons run without input |
| Versus battle | 🟢 Operational — playable with a controller; all 130 fighters and 16 stages sampled; the fight's logic and draw steps and the AI match PCSX2 exactly at every frame compared |
| Story mode | 🟢 Operational — all 200 parts and 73 towns run without input; not yet played through |
| Audio | 🟢 Operational — music, voices and effects; confirmed by listening |
| Saves | ✅ Verified — loading, saving and overwriting; the saves byte-identical to the original's in PCSX2 |
| Enhancements | 🟢 Operational — HD textures, super-sampling to 16x, widescreen, 60 fps battles, fast loading, beam struggles by buttons; full speed with all of them |
| Online battle | 🟢 Operational — lockstep over UDP with connection codes, no port to open; played between two homes; each player sees their own view, radar and prompts; rounds start locked on |

**Code:** 11,139 EE functions plus 494 in the `DBZP.BIN` overlay, all compiled natively; nothing interpreted in the gate's runs · **Gate:** unit tests, Python suites, 31/31 comparisons with the original, the battle run against its baseline.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Latest

- **2026-11-12 — loaded data against PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), step 5): at the first frame of the same versus battle, the loaded fighters and stage (22 MB from `0x700000`) equal PCSX2's but for 2.7 KB of positions and leftovers; the rest differs by route (the port's starter skips the title), the streams' timing and the kernel. A finding: sticks rest at `0x80` in the port, `0x7F` in PCSX2.
- **2026-11-12 — press to screen against PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), step 9): windowed, a press shown in 104 ms median by the port with Low input lag (the default), 108 ms by PCSX2, 111 ms by the port at 60 fps, 133 ms by the port without Low input lag.
- **2026-11-12 — the game's input delay equal to PCSX2's** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), step 9, first part): a press (Cross, d-pad up) at the same fight frame changes the display lists in the frame that read it and VU1's memory a frame later (2 and 4 fields), in PCSX2 and the port alike. Measured in PCSX2 with logging sites that do not wait, so its timing stays the game's.
- **2026-11-12 — 300 consecutive fight frames equal to PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md), step 3): every call of the fight frame the replay can run, at frames 600 to 899 of a versus battle, gives PCSX2's words (1,459 calls; 41 stop at the sound requests' system call, a service boundary). The replay now loads the game's random numbers (VU0's R register) where the runtime keeps them.
- **2026-11-12 — one comparison with PCSX2** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md)): the port runs the game's code from PCSX2's own state, compared byte for byte. It found three faults in the recompiled code (`sqrt.s` read the wrong register and must round to nearest; `movn`/`movz` copied 128 bits), now fixed. Since then the fight frame's logic and draw steps match PCSX2 at every frame compared, a full versus battle to its K.O. included (about 330 calls).
- **2026-11-12:** the Namek lava fix at 60 fps (the user still sees a flicker, below); HD packs found in nested folders; the launcher as Play / Online / Settings; one window to an online battle; no freeze on the online connect screen.
- **2026-11-11:** online rounds start locked on; "Low input lag" (a picture shown when drawn, 18.7 ms sooner in a 30 fps fight); fighters' HD textures no longer pop.
- Earlier: [November](progress/2026-11.md), [October](progress/2026-10.md).

## Open reports (the user's, not yet investigated)

- **The destroyed Namek still flickers** at 60 fps after the lava fix ([60 fps plan](plans/graphics/HIGH_FRAME_RATE_PLAN.md)).
- **Frieza over the ocean with HD:** a doubled outline, broken lines, blocky water ([HD textures plan](plans/graphics/HD_TEXTURES_PLAN.md)).
- **Characters under water** drawn without HD textures (same plan).
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
| IOP services | 🟢 Operational | Native disc, file, controller, memory card, sound and CRI services; no IOP code runs ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS and display | 🟢 Operational | paraLLEl-GS on its own threads, native passes for the outline, glow and depth of field |
| GIF / VIF / VU | 🟢 Operational | DMA as the game uses it; VU1 programs native, checked in the gate |
| IPU / video | 🟢 Operational | Both films decode through the game's Sofdec library |
| Audio | 🟢 Operational | ADX streams and a native effects sampler, 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Operational | Keyboard, XInput and DualSense; mapping in the launcher and the in-game menu |
| Memory card | ✅ Verified | A host folder per card; saves byte-identical to the original's ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Verified | ISO reads; archive table and overlay byte-identical to the original |

## Next

In the [implementation plan](project/IMPLEMENTATION_PLAN.md)'s order:

1. **The PCSX2 comparison** ([plan](plans/validation/PCSX2_COMPARISON_PLAN.md)): the rest of loaded data (the globals by route, the sticks' rest value), then VU1, GS memory and pictures, the services.
2. **The open reports** above, as the user gives detail.
3. **Picture and speed:** an edge filter, 60 fps load work, responsiveness steps 7 and 8 (waiting for the user's go).
4. **Later:** textures the HD pack lacks upscaled by the port ([plan](plans/graphics/TEXTURE_UPSCALING_PLAN.md)); the exhaustive battle sweep.
5. **The user's decisions:** a matchmaking server and rollback for online; publishing the release.

**Not compared with the original yet:** other screens' logic, loaded data at other events than a fight's first frame, VU1 memory, GS memory and pictures, the services after the boot.

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md)
