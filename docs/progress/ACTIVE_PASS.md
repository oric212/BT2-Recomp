# Resume handoff

Updated: 2026-10-20. **Pass in progress: live 60 fps in-between pictures** ([plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), step 3).

## Where the work stands

The whole game runs without input (all modes, all 200 story parts, 73 towns; [coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)); what each part was checked on is in [VALIDATION.md](VALIDATION.md). The enhancements HD textures (pack 2), super-sampling and widescreen are settings in the launcher and the in-game menu; the picture goes to the window on the GPU; a fight holds 30 pictures per second with every enhancement on.

**60 fps (this pass).** The engine steps a fixed amount 30 times a second (uncapped, the same fight runs at double speed), so the fight stays at 30 and in-between pictures are drawn from the game's own commands: the frame's VIF1 list with the camera and bone matrices halfway. Done: two frames' lists pair code for code; one in-between picture drawn offline, clean, with the game untouched. Built and tested: `runtime/vu/vif1_blend.*` (blending two frames' lists, `bt2-vif1-blend-tests`) and a second renderer on the same GPU device (`GpuGsBackend::initShared`, the HD pack shared, `showPictureOf`). Next:
1. Record each frame's GS-bound inputs in order (VIF1 lists with the GS bytes they produce, other GIF transfers).
2. A second pipeline (its own VIF1/VU1 copy, GS sink, texture names, the shared renderer): per frame the in-between picture of the last two frames, then the real frame so its state follows; first on the game's thread, checked by snapshots (the real replay must equal the main picture), then on a worker thread.
3. Show the in-between picture in the field between two fight flips (the list built during flip F appears at F + 2, so it adds no delay); then the setting "60 fps battles" in the launcher and the overlay.

**After this:** FSR 1 scaling with sharpening and an edge filter ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)); HD textures for the fighters' bodies (their palettes are drawn on the GPU) and the button art choice ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)); the final battle sweep; the runtime generalization plan's groups 4 to 6 at the very end (the user's decision).

**Waiting for the user** (built, gate passed, not confirmed in play): the enhancements in the launcher; the sound fix (speech was heard on the left only); fast forward; saving; special moves; the films with and without a skip; the launcher's screens; the camera in fights.

**Not compared with the original yet:** memory card replies, GS readback pixels, 3D frames, VU1 memory. The README's system requirements are estimates from this PC's measurements.

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (in `build\probe\Release`), `data` holds the disc image and `data\textures` the texture pack (pack 2 is linked there on this PC); `saves` holds the cards; the settings stay in `user\settings`, the last run's messages in `user\last-run.log`.
- Helper scripts in `work\scripts\` (`work\WORKING_WITHOUT_AI.md`): `play.cmd`, `discover.cmd` (a stop at untranslated code: maps it and rebuilds), `rebuild.cmd`, `gate.cmd`, `show-log.cmd`.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (builds, unit tests, Python suites, probe runs with the original comparisons, the Release battle run against `5a4960667d4a3dff`).
- **Without input:** `BT2_START_BATTLE`, `BT2_START_SCREEN`, `BT2_START_EPISODE` with `--pad-script ""`; sweeps: `battle_sweep.py`, `story_sweep.py`, `screen_run.py` (at most 6 jobs).
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>;…"`. **Audio:** `BT2_AUDIO_WAV=<absolute path>`. **Did the game change:** `BT2_STATE_HASH` on both runs, then `compare_state_hash.py`.
- **Picture settings:** `BT2_HD_TEXTURES`, `BT2_SUPER_SAMPLING`, `BT2_GAME_PATCHES=widescreen`. **60 fps study:** `BT2_VIF1_DUMP`, `BT2_VIF1_REPLACE`, `BT2_FIGHT_UNCAPPED`. **Cost:** `BT2_VIEWER_STATS`, `BT2_GPU_TIMESTAMPS`, `BT2_SAMPLE_PROFILE` ([TOOLS.md](../guides/TOOLS.md)).
- **Build times:** a runtime header about 16 s; everything about 6 minutes after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`.

## Rules learned the hard way

- **Build the player programs before handing a build over;** a build fails to link while a game window is open.
- **Runs with a window appear on the user's screen** (`BT2_VIEWER_CLOSE=1`); runs with `--sound` play on the user's speakers.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **After a change to `prepare_runtime.py`:** run it (and `generate_interpreter.py --reuse`), then build.
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
- **Commits carry no assistant attribution.**
