# Resume handoff

Updated: 2026-10-21. **No pass in progress.** HD textures cover the fighters (detail over the game's colours); next the CPU renderer's removal. The user's play test of 2026-10-21 found four faults, fixed (below); The last pass drew live 60 fps in-between pictures (headless; [plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), step 3).

## Where the work stands

The whole game runs without input (all modes, all 200 story parts, 73 towns; [coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)); what each part was checked on is in [VALIDATION.md](VALIDATION.md). The enhancements HD textures (pack 2), super-sampling and widescreen are settings in the launcher and the in-game menu; the picture goes to the window on the GPU; a fight holds 30 pictures per second with every enhancement on.

**60 fps battles — built 2026-10-21, a setting** ([plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), step 3): in-between pictures drawn by a second renderer on a worker thread from the main renderer's GPU state and the game's own commands with blended VIF1 lists (aligned unpack by unpack); exact by check (`BT2_SIXTY_FPS=check`); a fight holds 30 flips and 60 pictures per second, also with 16x, HD textures and widescreen (about 30 ms of work per flip of 33 there: little room). Left: things that appear in a frame show one picture early (the user's "pop-in"); a state-hash check that the fight is unchanged with the setting on; the user's look in play.

**HD textures across the whole game — built 2026-10-21** ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)): the fighters' bodies too. The game relights their palettes every frame, so exact PCSX2 names cannot cover them (22 to 29 % of draws even with palettes read back exactly; about 44 % of a fight's draws are a shading texture the pack lacks); the renderer draws the pack's texture as detail over the game's own colours (the user's direction), exact replacements as they are. Not yet seen in play; at 16x with 60 fps, HD and widescreen the GPU is near its limit in heavy moments (28.5 flips per second once; 8x holds 30).

**Next: remove the CPU renderer** (the user, 2026-10-21): the sink's rasterizer, the `cpu` and `mirror` modes and the CPU-drawn reference renders (to GPU snapshots); keep the packet checks and register tracking the GPU path and the texture names use.

**After this:** FSR 1 scaling with sharpening and an edge filter ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)); the button art choice ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)); the final battle sweep; the runtime generalization plan's groups 4 to 6 at the very end (the user's decision).

**Play test fixes of 2026-10-21** (gate passed, not yet confirmed in play): the in-game menu froze the game (the picture read for its background waited for a command buffer the renderer still held between flips; it is now flushed first); input lag (the pad was read before the field's wait, so in fields without a flip the game got input up to a field old; it is now read after the wait, game-visible order unchanged); thin lines pulsing in a stretched picture (Cell Games tiles: the presenter's filtered stretch now enlarges by whole multiples first, then filters the rest); integer scaling with widescreen and 4x or more super-sampling showed the picture at half size (window and picture widths now both from the height). Measured: a menu press shows in the picture flipped four fields later (Options screen, pad script); the game's own pipeline, as on the PS2.

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
