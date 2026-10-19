# Resume handoff

Updated: 2026-10-19. **No pass in progress.** The user asked to stop after this pass and not to start the story sweep; the next pass starts only when they say so.

## Where the work stands

Current plan: [whole-game coverage](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), ahead of performance. Done: the map closed statically, the interpreter bridge, the state hash, the census of what the game asks of the runtime, and game states started without input (`runtime/probe/game_starter.cpp`, tool runs only):

- `BT2_START_BATTLE=<character>[:<variant>],<character>[:<variant>],<stage>[,<music>]`: a CPU-vs-CPU battle (characters 0 to 129, variants 0 to 8, stages 0 to 15).
- `BT2_START_SCREEN=<mode>`: one of the ten main-menu screens.
- `BT2_START_EPISODE=<episode>[,<part>[,marker:<m>]]`: a story part without input: its map, the scene the marker leads to, its text boxes, the pre-battle screen, the battle and the results (no save is written); the run ends after the part's after-battle step. `tools/ps2recomp/story_scenes.py` lists the parts' markers and scenes; `tools/ps2recomp/story_sweep.py --jobs 4` runs the parts.

What each has been checked on: [VALIDATION.md](VALIDATION.md) (add every new check there).

**Next (when the user starts the next pass):**

1. **Story mode** ([plan](../plans/ee/STORY_MODE_PLAN.md), "Next steps"): every part's battle can now be reached without input (200 of 207 parts; a sample of 8 all finished their battle). Next: the story sweep of the 200 parts (`story_sweep.py --jobs 4`, about an hour), when the user starts it; then the player's side under CPU control so parts can be cleared; then optional markers, the shop and chases.
2. **Runtime special cases** (coverage plan, step 5): generalize each gated condition or unhandled system call when a run reaches it; check that the scripted fight's state hash is unchanged.
3. **Final validation** (the user's decision of 2026-10-19): the exhaustive battle sweep (`tools/ps2recomp/battle_sweep.py`: every character, variant and stage, about 70 s per battle, resumable) comes at the end.

**Waiting for the user's test** (built, gate passed, not confirmed in play): special moves and ultimates; saving (save, quit, start again); the films with and without a skip; the launcher's card list, controls and window size; the camera in fights.

**Open:** "mono sound sometimes" (not reproduced; ask where and when, music or effects). Not compared with the original yet: memory card replies, GS readback pixels, 3D frames, VU1 memory.

## How the user runs things

- Helper scripts in `work\scripts\` (local; `work\WORKING_WITHOUT_AI.md` describes them): `play.cmd` (the launcher), `discover.cmd`, `rebuild.cmd`, `gate.cmd`, `show-log.cmd` and others. Settings: `user/settings`; the last run's messages: `user/last-run.log`.
- **A stop at untranslated code** ("Branch target" in the log, or the game's message after a session that ran interpreted code): `work\scripts\discover.cmd` (`discovery_loop.py --player-stop`) maps it and rebuilds.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (builds, unit tests, the Python suites, the probe runs with the original comparisons, the Release battle run against `5a4960667d4a3dff`).
- **Scripted fight:** `run_startup_probe.py --config Release --frames 5100 --pad-script "<tools/gate/pad-fight.txt>"`. Pad scripts count fields; snapshots count flips.
- **Without input:** the starters above with `--pad-script ""`; `BT2_STALL_FIELDS=600` ends a run that stops presenting frames.
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>;…"` (PPM). **Audio:** `BT2_AUDIO_WAV=<absolute path>`.
- **Did a change alter what the game sees:** `BT2_STATE_HASH=<file>` on both builds, then `tools/ps2recomp/compare_state_hash.py`.
- **Tracing:** `BT2_EE_CALL_TRACE`, `BT2_EE_WATCH_QUADWORD`, `BT2_EE_WATCH_FLOAT` ([TOOLS.md](../guides/TOOLS.md)).
- **Build times:** a runtime header about 16 s; everything about 6 minutes after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`.

## Rules learned the hard way

- **Build the player programs before handing a build over:** `python tools/build/msvc.py cmake --build build/probe --config Release`; a build fails to link while a game window is open.
- **Runs with a window appear on the user's screen:** use `BT2_VIEWER_CLOSE=1`.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`); a change that alters the 3D output on purpose updates `BASELINE` there.
- **After a change to `prepare_runtime.py`:** run it (and `generate_interpreter.py --reuse`), then build.
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
- **Commits carry no assistant attribution** (no Co-Authored-By lines).
