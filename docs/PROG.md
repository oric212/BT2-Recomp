# Current progress

BT2 Recomp is a port of *Dragon Ball Z: Budokai Tenkaichi 2* (USA version, SLUS-21441) to Windows. The game's own
code is translated into C++ and runs as an ordinary PC program, on a small layer that stands in for the PS2. It is
not an emulator. The [implementation plan](project/IMPLEMENTATION_PLAN.md) explains the approach.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg?v=fc124425)

| Part of the game | Status | What this means |
| --- | --- | --- |
| Startup | ✅ Matches the original | The game starts and reaches its main loop. Its state at key moments is the same as in PCSX2. |
| Controller and memory card checks | ✅ Matches the original | The checks at boot work, and the "no memory card" message appears as on the PS2. |
| Films | ✅ Matches the original | The logo and opening films play with sound. Every picture is decoded exactly as PCSX2 decodes it. |
| Title and menus | ✅ Matches the original | The main menu, the Dueling menu and character selection behave exactly as in PCSX2. Every mode has been played. |
| Versus battle | ✅ Matches the original | A fight runs exactly as in PCSX2, frame by frame. All 130 fighters and all 16 stages have been run. |
| Story mode | 🟢 Tested | Partly played. All 200 story parts and 73 towns have been run. |
| Sound | 🟢 Tested | Music, voices and effects are mixed the way the game's own sound driver mixes them. Checked by listening. |
| Saves | ✅ Matches the original | Save files are identical, byte for byte, to the ones the original game writes. |
| Enhancements | 🟢 Tested | HD textures, super-sampling up to 16x, widescreen, 60 fps battles, fast loading and quick start. |
| Online battle | 🟢 Tested | Two players on different PCs fight over the internet, joined by a code. Played between two homes. |

**Code:** all 11,139 functions of the game's main program, and the 494 in its `DBZP.BIN` file, are compiled to
native code. **Checks:** every change must pass the gate before it is committed: the unit tests, 31 comparisons of
startup with the original game, and a scripted fight whose result must equal a recorded one.
<!-- progress-summary:end -->

What the statuses mean ([full definitions](project/PROGRESS_TRACKING.md), [how each part was checked](progress/VALIDATION.md)):

- ✅ **Matches the original:** compared with the original game running in PCSX2, for the part described.
- 🟢 **Tested:** played or listened to on several PCs, but not compared with PCSX2.
- 🔵 **Runs:** reached by automated runs only.
- 🔧 **Incomplete**, ⛔ **Blocking**, ⏳ **Not reached yet.**

## Where things stand

Every mode of the game can be played. The current release is
[v0.2](https://github.com/oric212/BT2-Recomp/releases/tag/v0.2). It works with a keyboard, Xbox controllers,
DualSense, DualShock 4 and most other gamepads.

The port has been checked against the original game running in PCSX2, the PS2 emulator. The fight, the menus
checked so far and the PS2 services all match, and wherever something differs, the reason is known. The next
section has the details.

Besides the development PC, the port has been played on several other PCs, and online between two homes.

What remains are a few graphics glitches that players reported (see [Open reports](#open-reports)).

## Comparison with PCSX2

**How it was done:** PCSX2 runs the original game and pauses at a chosen moment. The port then loads PCSX2's exact
state from that moment, runs the same piece of game code, and the two results are compared byte for byte. Because
both start from the same state, any difference is a fault in the port and not a matter of timing.
The [comparison page](plans/validation/PCSX2_COMPARISON_PLAN.md) has the full plan and record.

| What was compared | Result |
| --- | --- |
| The fight's logic | Every step of a fight frame is identical, for 300 frames in a row and for a whole battle. |
| The fight's graphics | The graphics programs (VU1) leave identical memory, and all textures are identical. The finished pictures differ only by small rounding differences between the two renderers. |
| Memory when a fight starts | The fighters' and the stage's data are identical. Every other difference has a known, harmless cause. |
| PS2 services (disc, sound, memory card, controllers) | From boot to the title demo, every answer the game receives is identical or the difference is explained. |
| Menus | The main menu, the Dueling menu and character selection are identical. |
| Input lag | The game's own delay is the same. From button press to screen: 104 ms in the port, 108 ms in PCSX2. |

The comparison was finished on 2026-11-16, because it covers everything the port needs. Two optional items are left
for later: comparing the story map and a town, and adding the stored fight comparisons to the gate.

It found eight faults, all now fixed: three in how CPU instructions were translated (`sqrt.s` twice, and
`movn`/`movz`), one in the VU1 graphics programs, three in the answers of the port's PS2 services, and one in the
film decoder's rounding.

## Latest

- **2026-11-16: v0.2 released.** Voices were far too quiet. They are now mixed the way the game's sound driver mixes
  them, with each voice's own volume and position, and they have their own volume slider. The release also adds the
  DualShock 4 and generic gamepads, and it was tested on several PCs and online.
- **2026-11-16: more controllers.** The DualShock 4 and generic gamepads (for example 8BitDo pads in D-input mode)
  now work alongside Xbox controllers and the DualSense. Two players can use any mix of them.
- **2026-11-16: comparison with PCSX2 finished.** The last unexplained differences at the start of a fight were
  explained or fixed; the film decoder now rounds the way PCSX2 does. PCSX2 can now press buttons from a script and
  reach the menus by itself; the main menu, Dueling and character selection match.
- **2026-11-13: v0.1 released.** The first download, with a player's guide and a list of known problems. It added
  **Quick start**, which goes straight to the main menu with your save loaded.
- **2026-11-12 and 13: the fight compared with PCSX2.** The fight's logic and graphics match. Four translation
  faults were fixed, and after three fixes the services' answers match from boot to the title demo.
- Older entries: [November](progress/2026-11.md), [October](progress/2026-10.md).

## Open reports

Problems that players reported and that are not fixed yet:

- At 60 fps, the **destroyed Namek** stage still flickers, even after the lava fix
  ([60 fps plan](plans/graphics/HIGH_FRAME_RATE_PLAN.md)).
- With HD textures, **Frieza over the ocean** shows a doubled outline, broken lines and blocky water
  ([HD textures plan](plans/graphics/HD_TEXTURES_PLAN.md)).
- With HD textures, **characters under water** are drawn without them (same plan).
- Online, **"Press O to recover"** sometimes does not appear. No fault has been found in the port yet. It would help
  to know which fighters, which attack and which side ([online plan](plans/online/ONLINE_BATTLE_PLAN.md)).
- A texture looked wrong once in the first fight with HD textures. It has not happened again.

**Not yet tried by a player:** the new controllers (DualShock 4, 8BitDo in D-input mode), online between two PCs with
the release download, Low input lag, Variable refresh rate, beam struggles by buttons, and a second controller.

## Measurements

Measured on one PC (Core i9-10900K, Radeon RX 9070 XT) with the Release build, during a fight with every
enhancement on. More in the [performance findings](reference/PERFORMANCE_FINDINGS.md).

| What | Result |
| --- | --- |
| Game speed | 29.97 frames a second, the PS2's own speed, including during super attacks. |
| 60 fps battles | One extra picture between each two game frames, 6.3 ms to draw. |
| Processor time for the game | 10 to 14 ms of each 33 ms frame. |
| Graphics card time | 6 ms a frame with no enhancements; 8 to 10 ms with 16x super-sampling, HD textures and widescreen. |
| Button press to screen | 104 ms with Low input lag on (PCSX2: 108 ms); 111 ms at 60 fps; 133 ms with Low input lag off. |
| Memory | 0.7 to 0.85 GB of RAM, and 0.8 to 1.2 GB of video memory. |

## Parts of the port

For developers: the PS2's parts, and how the port handles each one.

| Part | Status | What it is, and how the port handles it |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Tested | The PS2's main processor and its system calls. Every game function is compiled to native code. Floating-point rounding and register sizes were checked against PCSX2. Threads, semaphores and interrupts are handled natively. |
| IOP services | ✅ Matches the original | The PS2's second processor handles the disc, files, controllers, memory card, sound and the CRI middleware. The port answers these requests itself and runs no IOP code. Its answers equal PCSX2's from boot to the title demo ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)). |
| GS and display | 🟢 Tested | The PS2's graphics chip. Its commands are drawn on your graphics card by paraLLEl-GS, on its own threads. The outline, glow and depth-of-field effects have their own native passes. |
| GIF / VIF / VU | ✅ Matches the original | The paths that carry graphics data to the GS, and the VU1 vector unit that prepares it. The VU1 programs run natively, and VU1's memory equals PCSX2's in fight frames; the gate checks this. |
| IPU / video | ✅ Matches the original | The PS2's video decoder. Both films are decoded by the game's own Sofdec video library, with PCSX2's arithmetic. |
| Sound | 🟢 Tested | Music and voice streams (ADX) are mixed the way the game's CRI sound driver mixes them, and a native player plays the sound effects, at 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)). |
| Input | 🟢 Tested | Keyboard, Xbox controllers (XInput), and the DualSense, DualShock 4 and generic gamepads (HID). Buttons can be changed in the launcher and in the in-game menu. |
| Memory card | ✅ Matches the original | Each memory card is a folder on the PC. Saves are byte-identical to the original's ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)). |
| Disc and files | ✅ Matches the original | The game is read from your disc image. Its archive table and its `DBZP.BIN` file load byte-identical to the original. |

## Next

1. **The open reports** above, as players give more detail.
2. **Picture and speed:** an edge filter, less processor and graphics load at 60 fps, and shorter input lag.
   These wait for the go-ahead.
3. **Later:** HD versions of the textures the HD pack lacks, upscaled by the port
   ([plan](plans/graphics/TEXTURE_UPSCALING_PLAN.md)), and an automated run of every fighter on every stage.
4. **Decisions for the project owner:** a matchmaking server and rollback for online play.

[Active handoff](progress/ACTIVE_PASS.md) · [Validation record](progress/VALIDATION.md) · [components.json](progress/components.json) · [Reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [Runtime layout](guides/RUNTIME_LAYOUT.md) · [Tools](guides/TOOLS.md)
