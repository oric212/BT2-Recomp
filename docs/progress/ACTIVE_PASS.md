# Resume handoff

Updated: 2026-10-20. **No pass in progress.** The last pass ran all 15 Ultimate Battle Z courses and repeated Dragon Tournaments without input (the screen starter takes the courses in turn, gives side 1 one health bar, answers the screens' "Exit saving?" with Yes), wrote the enhancement plans (HD textures, image quality and widescreen, 60 fps battles), built step 1 of the HD textures plan (PCSX2 texture names, `BT2_TEXTURE_NAMES`, `tools/graphics/textures/texture_name_check.py`; the check against the packs is not run yet) and fixed fast forward (off when the window loses the focus, the sound queue refilled when it ends, pictures shown at most once per field period, the mark cleared at once; `BT2_FAST_FORWARD`).

## Where the work stands

Current plan: [whole-game coverage](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), ahead of performance. Done: the map closed statically, the interpreter bridge, the state hash, the census of what the game asks of the runtime, and game states started without input (`runtime/probe/game_starter.cpp`, tool runs only):

- `BT2_START_BATTLE=<character>[:<variant>],<character>[:<variant>],<stage>[,<music>]`: a CPU-vs-CPU battle (characters 0 to 129, variants 0 to 8, stages 0 to 15).
- `BT2_START_SCREEN=<mode>`: one of the ten main-menu screens.
- `BT2_START_EPISODE=<episode>[,<part>[,marker:<m>]]`: a story part without input: its map (a chase's enemy reported caught), the scene the marker leads to, its text boxes, the pre-battle screen, the battle with the CPU on both sides and the results (no save is written); the run ends after the part's after-battle step. From an optional marker's town it visits every character (talk, items, the shop, fights). `tools/ps2recomp/story_scenes.py` lists the parts' markers and scenes; `tools/ps2recomp/story_sweep.py --jobs 6` runs the parts (about an hour; 4 of the 10 cores stay free for the user); `--optional` runs the 73 towns (about 25 minutes).

What each has been checked on: [VALIDATION.md](VALIDATION.md) (add every new check there).

**Next (when the user starts the next pass):**

1. **Story mode** ([plan](../plans/ee/STORY_MODE_PLAN.md)): done as far as the starter reaches: all 200 parts (62 cleared, 138 lost in the last sweep, `work/coverage/story/rules/battles.json` holds each battle's rule, rewards, sagas and Dragon Ball), the 73 towns, saga unlocks and a Dragon Ball pickup seen. Left: the plan's "What is missing" (the 18 saga rewards and the ball rewards need wins the CPU did not get).
2. **The other modes' battles** ([coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), "Step 6: what is left"): done 2026-10-20: all 15 Ultimate Battle Z courses, the Dragon Tournament through its save question and the 21 Ultimate Training lessons run without input (`tools/ps2recomp/screen_run.py`).
3. **Runtime special cases** ([runtime generalization plan](../plans/ee/RUNTIME_GENERALIZATION_PLAN.md)): the startup's conditions on pcs, stack offsets, call counts and phases are replaced by the general kernel and hardware behavior group by group; groups 1 (kernel semaphores), 2 (the GS reset path; its two VSync waits moved to group 4) and 3 (the startup's thread calls, thread exit and deletion; blocking semaphore waits are reached by nothing and stay explicit stops) done; groups 4 to 6 (the startup's own replays) wait until after the final validation (the user's decision of 2026-10-20). Each group: unit tests, the gate (32/32 comparisons, battle hash unchanged), a sample of the sweeps.
4. **Final validation** (the user's decision of 2026-10-19): the exhaustive battle sweep (`tools/ps2recomp/battle_sweep.py`: every character, variant and stage, about 70 s per battle, resumable) comes at the end.
5. **Enhancements** (the user, 2026-10-20; HD textures first): [HD textures](../plans/graphics/HD_TEXTURES_PLAN.md): the name check is done and the replacements are drawn (`BT2_HD_TEXTURES`; pictures for the user in `work/frames/hd-textures/`); super-sampling with high-resolution output works (`BT2_SUPER_SAMPLING`, 1,280 × 896 at 4× and more); the launcher's and the overlay's Display page has HD textures and Super-sampling (from the next start; pack 2 is linked into `build/probe/Release/data/textures` on this machine); widescreen built (2026-10-20: the SLUS-21441 16:9 patch plus the story map's projection, `BT2_GAME_PATCHES=widescreen`, Display's "Widescreen"; pictures in `work/frames/widescreen/`). Order chosen 2026-10-20 (the user asked for performance, 60 fps and widescreen): widescreen, then performance (the [plan](../plans/ee/PERFORMANCE_PLAN.md)'s presenting on the GPU: done 2026-10-20; the waits at high super-sampling (frames in flight: 8 frame contexts) and the HD naming fixed 2026-10-20: 16x with pack 2 at 30 per second in the window (item 4a); next 60 fps by in-between pictures drawn from the game's commands (the [plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md)'s step 1 done 2026-10-20: two frames' VIF1 streams pair code for code, only bone and camera matrices change; step 2 done the same day: an in-between picture drawn offline from the game's commands, the game untouched; next step 3, live in-between pictures in their own GS memory), graphics on its own thread and the slow single pictures; the user: no CPU or GPU bottleneck, enhancements must not change the fight's speed or physics), then [60 fps battles](../plans/graphics/HIGH_FRAME_RATE_PLAN.md) (its steps 1 and 2, the uncapped fight and the time scale, can be measured before), each with its launcher and overlay setting; then the drawn character palettes, the button art choice; [image quality](../plans/graphics/IMAGE_QUALITY_PLAN.md) steps 3 and 4. The runtime generalization plan's groups 4 to 6 come at the very end.

**Waiting for the user's test** (built, gate passed, not confirmed in play): HD textures, super-sampling and widescreen in the launcher; special moves and ultimates; saving (save, quit, start again); the films with and without a skip; the launcher's card list, controls and window size; the camera in fights.

**Open:** sound in the right ear only until a fight is run (the user, 2026-10-20; earlier "mono sound sometimes"): not reproduced: WAV recordings of the boot, the opening film, the title, the menus, the Ultimate Battle Z screens and the story map are balanced (left and right within about 10% outside the story map's music, which is about 30% louder on the left; no 0.1-second stretch with one side below a ninth of the other), and no one-channel stream played. Next: a recording of the user's own session (`BT2_AUDIO_WAV=<file>` passes through `bt2-game`). Fast forward: fixed and measured (a fight at 1.6 times its speed, limited by the fight's own work); to be tried by the user. Not compared with the original yet: memory card replies, GS readback pixels, 3D frames, VU1 memory.

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (in `build\probe\Release`), `data` holds the disc image (a hard link to `rom\...iso` there now) and `saves` the cards (copied from `user\memcards` on the first start); the settings stay in `user\settings`.
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
