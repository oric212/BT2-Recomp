# Resume handoff

Updated: 2026-10-13. **No pass in progress.**

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. Every mapped function (10,682, plus the 473 of the `DBZP.BIN` overlay) is compiled into the probe. The long Release run with scripted input goes from the title screen through the menus, character and map selection and loading into a 1P-vs-COM battle and ends at its 9,000-frame budget (4,936 / 4,927 functions). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next passes

Order set by the user on 2026-10-13, per [PROG.md](../PROG.md#next-steps):

1. **Sound output:** ADX decoding and sound bank playback behind the sound and CRI services ([sound plans](../plans/sound/)). Start by listing what the services already receive in the battle run (stream announcements, SOUNDS requests) and writing the plan for the output path.
2. **Saves:** a memory card image behind the native card service. The game asks "Do you wish to create a save file on the memory card (PS2)?" after New Game; the script answers No. The Yes path is untested.
3. **Controller support:** host input (keyboard, XInput) into the native controller service.
4. **Play-testing by the user:** live input in the viewer, recorded as a pad script; a session that stops at a boundary is replayed through `discovery_loop.py`. This replaces scripted presses and screenshots for the other menu entries and story mode. Battles run at about 19 presented frames per second of wall-clock time, so the VU1 path needs a performance pass before live play in fights.

## Frames to inspect

Saved under `work/frames/` (local, ignored), from the long Release run on the GPU renderer; `battle-sheet.png` shows them together:

- `battle-2100.png`, `battle-2400.png`: the two New Game dialogues and the main menu.
- `battle-2800.png`, `battle-3300.png`: the Dueling menu and character selection.
- `battle-3600.png` to `battle-4450.png`: character selection steps, map selection, the loading screen.
- `battle-4700.png` to `battle-8990.png`: stage fly-over, intro poses and the fight.

The user confirmed the title dialogues, the menus and character selection on 2026-10-13. Map selection, the loading screen and the battle frames are not confirmed yet.

## How to run

- **Battle run** (about 4.5 minutes): `run_startup_probe.py --config Release --frames 9000 --pad-script "<contents of work/bt2-generation/pad-fight.txt>"`. The pad scripts are local and ignored: `pad-new-game.txt` (to the main menu), `pad-duel.txt`, `pad-versus.txt`, `pad-select.txt` (character selection), `pad-battle.txt` (Cross every 150 fields from 4200 to 6000: through the selections into the fight), `pad-fight.txt` (the same plus Up and Square during the fight).
- **Writing a pad script:** Start opens the title's option list and Cross confirms. Scripts count fields; snapshots count flips and print `Snapshot: flip N at field M`. Menus present one frame per field (field = flip + about 700 after the movies), the battle one per two fields.
- **Frames:** `BT2_GPU_GS_SNAPSHOT="<flip>:<path>;<flip>:<path>"` writes those flips as PPM.
- **Discovery loop:** `python tools/ps2recomp/discovery_loop.py --disc <iso> --frames <n> --pad-script-file <file>` ([TOOLS.md](../guides/TOOLS.md)); its log is `work/bt2-generation/discovery-loop.log`. A stop it does not handle (not a map gap, or "map did not grow") is named in the log.
- **A hang:** `BT2_STALL_FIELDS=120` ends a run without display flips within seconds; a loop that never ends inside one field stops at the runaway guard (`BT2_EE_LOOP_LIMIT` to change its bound). A bad float: `BT2_EE_WATCH_FLOAT=<address>:<limit>`.

## Notes

- Build with `python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe` (Ninja). Times: runtime header (`ee_runtime.h`) edit about 16 s, one file about 12 s, full rebuild about 6 minutes (only after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`).
- Gate: `python tools/gate/run_gate.py --disc <iso>` (about 80 s plus the build).
- The 3D frames exist only on the GPU renderer: the CPU GS model refuses perspective texturing and fog, so `--gs cpu` and `--gs mirror` stop at the first 3D frame. The gate's CPU run ends before any 3D frame.
- GS readback is untested against the original: the destination buffers of the 12 store-image calls in battle loading are not compared with a PCSX2 capture. In GPU-only mode paraLLEl-GS supplies the pixels; the CPU model reads CT32 and CT24 only.
- VU1 counts one cycle per instruction pair without stalls. The report's `vu1_early_q_reads` and `vu1_early_clip_reads` are 0 in the battle run too.
- The battle presents one frame per two fields of game time. Whether the original runs this scene at 60 or 30 frames per second is not checked; if it is 60, look at what consumes the second field in the frame loop.
- SOUNDS functions `0x60`, `0x80` and `0x100` are answered with the unchanged status block by inference (not in the PCSX2 call log); capture them if a difference shows.
- The gate runs 22 comparisons on the full run and 9 at phase stops; the 15 that need their own inputs or an earlier stop, or that differ as recorded in [PROG.md](../PROG.md#verification-2026-10-13), are listed in `NOT_IN_GATE` in `tools/gate/run_gate.py`.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/`, `work/pcsx2-capture/sounds-log/` and `sounds-function-*`; reference movie frames in `work/movie-ref/`; the five VU1 microprograms of the first demo list in `work/vu1-programs/`.
