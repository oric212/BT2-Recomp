# Current progress

*Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441) as a native Windows x64 port: the game's own code recompiled to C++
on a small native layer, not an emulator ([implementation plan](project/IMPLEMENTATION_PLAN.md)).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg?v=fc124425)

| Part of the game | Status | In short |
| --- | --- | --- |
| Startup | ✅ Matches the original | Boots to the game's frame loop; key states equal to PCSX2's |
| Controller and card checks | ✅ Matches the original | Native services; the no-card prompt as the original's |
| Films | ✅ Matches the original | Logo and opening films with sound, decoded exactly as PCSX2 does |
| Title and menus | ✅ Matches the original | Main menu, Dueling and character selection equal to PCSX2; every mode played |
| Versus battle | ✅ Matches the original | The fight equal to PCSX2 frame by frame; all 130 fighters and 16 stages run |
| Story mode | 🟢 Tested | Partly played; all 200 parts and 73 towns run |
| Audio | 🟢 Tested | Music, voices and effects mixed as the original's driver does; confirmed by listening |
| Saves | ✅ Matches the original | Byte-identical to the original's |
| Enhancements | 🟢 Tested | HD textures, up to 16x super-sampling, widescreen, 60 fps battles, fast loading, quick start |
| Online battle | 🟢 Tested | Lockstep over the internet with connection codes; played between two homes |

**Code:** 11,139 game functions plus 494 in the `DBZP.BIN` overlay, all compiled natively · **Checks:** every change passes the gate (unit tests, 31 startup comparisons with the original, a scripted fight against its recorded result).
<!-- progress-summary:end -->

Statuses, by evidence: ✅ Matches the original (compared with PCSX2 for the scope noted) · 🟢 Tested (played or
listened to by the user, on several PCs; not compared) · 🔵 Runs (reached by automated runs only) · 🔧 Incomplete ·
⛔ Blocking · ⏳ Not reached ([definitions](project/PROGRESS_TRACKING.md)). How each part was checked:
[VALIDATION.md](progress/VALIDATION.md).

## Where things stand

- **Playable:** every mode, with a keyboard, Xbox pads, DualSense, DualShock 4 or generic gamepads. The current release is
  [v0.2](https://github.com/oric212/BT2-Recomp/releases/tag/v0.2).
- **Checked against PCSX2:** the [comparison](plans/validation/PCSX2_COMPARISON_PLAN.md) is closed: the fight,
  the menus reached and the services equal PCSX2's or are explained.
- **Tested on several PCs** besides the development machine, and online between two homes.
- **Waiting on the player's reports:** a few graphics glitches (below).

## Comparison with PCSX2

The port runs the game's code from PCSX2's own state and the results are compared byte for byte
([plan and record](plans/validation/PCSX2_COMPARISON_PLAN.md)).

| Area | Status | Result |
| --- | --- | --- |
| Fight logic | ✅ Done | Every step of the fight frame equal, 300 consecutive frames and a full battle |
| Fight graphics (VU1, GS) | ✅ Done | VU1's memory and all textures equal; pictures within the two renderers' rounding |
| Memory at a fight's start | ✅ Done | Fighters and stage byte-equal; every other difference explained |
| Services (disc, sound, card, controllers) | ✅ Done | Boot to the title demo: equal or explained |
| Menus | ✅ Done | Main menu, Dueling and character selection equal |
| Input lag | ✅ Done | The game's own delay equal; press to screen 104 ms (port) against 108 ms (PCSX2) |

Closed on 2026-11-16: the plan was wider than the port needs. Optional, if wanted later: the story map's and a
town's logic, and the stored fight comparisons in the gate.

Faults it found, all fixed: three CPU instruction faults (`sqrt.s` twice, `movn`/`movz`), one in VU1, three in the
native services' replies, and the film decoder's rounding.

## Latest

- **2026-11-16 — v0.2 released:** voices fixed (streams mixed as the original's sound driver does: each player's
  volume and pan; speech had been far too quiet) with their own volume setting; DualShock 4 and generic gamepads;
  the PCSX2 comparison closed; the port played on several PCs and online.
- **2026-11-16 — controllers:** the DualShock 4 and generic gamepads (8BitDo in D-input mode and the like) work
  alongside Xbox pads and the DualSense; several at once for two players.
- **2026-11-16 — comparison with PCSX2:** the last unexplained differences at a fight's start are explained or fixed
  (the film decoder now rounds as PCSX2); PCSX2 sessions press buttons and reach the menus by script; the main menu,
  Dueling and character selection match PCSX2.
- **2026-11-13 — v0.1 released:** the first package, with a player's guide and known issues; **Quick start**
  (straight to the main menu with the save loaded).
- **2026-11-12 to 13 — the fight against PCSX2:** the fight's logic and graphics match; four emulation faults fixed;
  the services' replies equal from boot to the title demo after three fixes.
- Earlier: [November](progress/2026-11.md), [October](progress/2026-10.md).

## Open reports (the player's, not yet investigated)

- **The destroyed Namek still flickers** at 60 fps after the lava fix ([60 fps plan](plans/graphics/HIGH_FRAME_RATE_PLAN.md)).
- **Frieza over the ocean with HD:** a doubled outline, broken lines, blocky water ([HD textures plan](plans/graphics/HD_TEXTURES_PLAN.md)).
- **Characters under water** drawn without HD textures (same plan).
- **"Press O to recover" not always shown online:** no port fault found ([online plan](plans/online/ONLINE_BATTLE_PLAN.md)); which fighters, attack and side?
- A texture glitch in the first HD fight (not reproduced).

**Waiting for the player's try:** the new controllers (DualShock 4, 8BitDo in D-input mode); online on two PCs with
the release package; Low input lag; Variable refresh rate; beam struggles by buttons; the second controller.

## Measurements

One PC (Core i9-10900K, Radeon RX 9070 XT), Release build, a fight with every enhancement on
([performance findings](reference/PERFORMANCE_FINDINGS.md)).

| Item | Value |
| --- | --- |
| Fight speed | 29.97 frames a second, super attacks included; 60 fps adds a picture between frames, 6.3 ms each |
| Game thread | 10 to 14 ms of a 33 ms frame |
| Graphics card | 6 ms a frame plain, 8 to 10 ms with 16x, HD and widescreen |
| Press to screen | 104 ms with Low input lag (PCSX2: 108 ms); 111 ms at 60 fps; 133 ms without Low input lag |
| Memory | 0.7 to 0.85 GB; 0.8 to 1.2 GB of video memory |

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Tested | Every mapped function compiled; floating-point rounding and register widths checked against PCSX2; threads, semaphores, interrupts |
| IOP services | ✅ Matches the original | Native disc, file, controller, memory card, sound and CRI services; no IOP code runs; replies equal PCSX2's from boot to the title demo ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS and display | 🟢 Tested | paraLLEl-GS on its own threads; native passes for the outline, glow and depth of field |
| GIF / VIF / VU | ✅ Matches the original | DMA as the game uses it; VU1 programs native, VU1 memory equal to PCSX2's in fight frames, checked in the gate |
| IPU / video | ✅ Matches the original | Both films decode through the game's Sofdec library, with PCSX2's arithmetic |
| Audio | 🟢 Tested | ADX streams mixed as the original's CRI driver does and a native effects sampler, 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Tested | Keyboard, XInput pads, and over HID the DualSense, DualShock 4 and generic gamepads; remapping in the launcher and the in-game menu |
| Memory card | ✅ Matches the original | A host folder per card; saves byte-identical to the original's ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Matches the original | ISO reads; archive table and overlay byte-identical to the original |

## Next

1. **The open reports** above, as the player gives detail.
2. **Picture and speed:** an edge filter, 60 fps load work, further responsiveness steps (waiting for the go-ahead).
3. **Later:** textures the HD pack lacks, upscaled by the port ([plan](plans/graphics/TEXTURE_UPSCALING_PLAN.md)); a
   battle sweep over every fighter and stage.
4. **Decisions for the owner:** a matchmaking server and rollback for online.

[Active handoff](progress/ACTIVE_PASS.md) · [validation record](progress/VALIDATION.md) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md)
