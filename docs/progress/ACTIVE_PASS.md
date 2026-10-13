# Resume handoff

Updated: 2026-10-13. **No pass in progress.**

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. Every mapped function (10,682, plus the 473 of the `DBZP.BIN` overlay) is compiled into the probe. The long Release run with scripted input goes from the title screen through the menus, character and map selection and loading into a 1P-vs-COM battle and ends at its 9,000-frame budget (4,936 / 4,927 functions). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next passes

Order set by the user on 2026-10-13, per [PROG.md](../PROG.md#next-steps):

1. **Sound output** ([plan](../plans/sound/SOUND_OUTPUT_PLAN.md)). Done: stream audio (movies, music) at the level measured against the user's PCSX2 recording, and sound effects through a native sampler behind SOUNDS: one-shot and looping sounds, the keep and stop flags, slot volumes, silence and stop-all, as read from the module's code. The user listened to the run with one-shot effects and confirmed it. `work/audio/battle-run.wav` (menus and a fight, 239 s) now also has the looping sounds; the user is asked whether they start and end where they should. The probe plays sound on the default audio device with `--sound` (also with `--viewer`), and the device then holds the run at the game's speed; the user is asked to try it. Sound is then complete except for volume settings. The comparison with the original is an optional final step.
2. **Controller support** — done with a default mapping (keyboard: arrows, Z cross, X circle, A square, S triangle, Q/W L1/R1, E/R L2/R2, Enter start, Backspace select, C/V L3/R3; gamepad: by position). Play with `run_startup_probe.py --config Release --play [--record <file>]`: the viewer with sound and live input while its window has the focus; a pad script is ignored then. The user is asked to try an XInput pad and a DualSense, which no run has exercised.
3. **Launcher and settings** ([plan](../plans/launcher/LAUNCHER_PLAN.md)): next is the launcher shell (game and launcher programs, settings in `user/`), then the in-game overlay (volumes, fast forward, quit), then the Controls screen, then saves (a memory card image; the game asks "Do you wish to create a save file on the memory card (PS2)?" after New Game, the script answers No, the Yes path is untested), then HD textures. The user's answers of 2026-10-13 are in the plan's first section.
4. **Play-testing by the user:** live input in the viewer, recorded as a pad script; a session that stops at a boundary is replayed through `discovery_loop.py`. This replaces scripted presses and screenshots for the other menu entries and story mode. Battles run at about 19 presented frames per second of wall-clock time, so the VU1 path needs a performance pass before live play in fights.

## Frames to inspect

Saved under `work/frames/` (local, ignored), from the long Release run on the GPU renderer; `battle-sheet.png` shows them together:

- `battle-2100.png`, `battle-2400.png`: the two New Game dialogues and the main menu.
- `battle-2800.png`, `battle-3300.png`: the Dueling menu and character selection.
- `battle-3600.png` to `battle-4450.png`: character selection steps, map selection, the loading screen.
- `battle-4700.png` to `battle-8990.png`: stage fly-over, intro poses and the fight.

The user inspected all of them on 2026-10-13 and confirmed them, map selection, the loading screen and the battle included.

## How to run

- **Battle run** (about 4.5 minutes): `run_startup_probe.py --config Release --frames 9000 --pad-script "<contents of work/bt2-generation/pad-fight.txt>"`. The pad scripts are local and ignored: `pad-new-game.txt` (to the main menu), `pad-duel.txt`, `pad-versus.txt`, `pad-select.txt` (character selection), `pad-battle.txt` (Cross every 150 fields from 4200 to 6000: through the selections into the fight), `pad-fight.txt` (the same plus Up and Square during the fight).
- **Writing a pad script:** Start opens the title's option list and Cross confirms. Scripts count fields; snapshots count flips and print `Snapshot: flip N at field M`. Menus present one frame per field (field = flip + about 700 after the movies), the battle one per two fields.
- **Live sound:** `run_startup_probe.py --config Release --viewer --sound [--pad-script ...]` plays the audio while the viewer shows the frames. At the end the log has `Audio device: N fields played, ran dry before M, ...`; M counts gaps.
- **Audio:** `BT2_AUDIO_WAV=<absolute path>` writes the run's audio (streams and effects) as a 48 kHz stereo WAV. `BT2_SOUND_LOG=<existing folder>` writes every SOUNDS and sound driver request and each bank's blocks and samples there (local evidence; `work/audio/soundlog/` holds the battle run's).
- **Frames:** `BT2_GPU_GS_SNAPSHOT="<flip>:<path>;<flip>:<path>"` writes those flips as PPM.
- **Discovery loop:** `python tools/ps2recomp/discovery_loop.py --disc <iso> --frames <n> --pad-script-file <file>` ([TOOLS.md](../guides/TOOLS.md)); its log is `work/bt2-generation/discovery-loop.log`. A stop it does not handle (not a map gap, or "map did not grow") is named in the log.
- **A hang:** `BT2_STALL_FIELDS=120` ends a run without display flips within seconds; a loop that never ends inside one field stops at the runaway guard (`BT2_EE_LOOP_LIMIT` to change its bound). A bad float: `BT2_EE_WATCH_FLOAT=<address>:<limit>`.

## Notes

- Build with `python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe` (Ninja). Times: runtime header (`ee_runtime.h`) edit about 16 s, one file about 12 s, full rebuild about 6 minutes (only after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`).
- Gate: `python tools/gate/run_gate.py --disc <iso>` (about 80 s plus the build).
- The 3D frames exist only on the GPU renderer: the CPU GS model refuses perspective texturing and fog, so `--gs cpu` and `--gs mirror` stop at the first 3D frame. The gate's CPU run ends before any 3D frame. The user decided on 2026-10-13 that 3D on the CPU renderer is an optional extra for the end of the project; GS counts as Operational on the GPU renderer.
- GS readback is untested against the original: the destination buffers of the 12 store-image calls in battle loading are not compared with a PCSX2 capture. In GPU-only mode paraLLEl-GS supplies the pixels; the CPU model reads CT32 and CT24 only.
- VU1 counts one cycle per instruction pair without stalls. The report's `vu1_early_q_reads` and `vu1_early_clip_reads` are 0 in the battle run too.
- The battle presents one frame per two fields of game time. Whether the original runs this scene at 60 or 30 frames per second is not checked; if it is 60, look at what consumes the second field in the frame loop.
- SOUNDS requests `0x60` (output mode) and `0x80` (replace a bank) are refused by name; no run has reached them. The module's handlers are known from its code (sound plan, Evidence), disassembled locally to `work/audio/irx/`.
- The gate runs 22 comparisons on the full run and 9 at phase stops; the 15 that need their own inputs or an earlier stop, or that differ as recorded in [PROG.md](../PROG.md#verification-2026-10-13), are listed in `NOT_IN_GATE` in `tools/gate/run_gate.py`.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/`, `work/pcsx2-capture/sounds-log/` and `sounds-function-*`; reference movie frames in `work/movie-ref/`; the five VU1 microprograms of the first demo list in `work/vu1-programs/`.
