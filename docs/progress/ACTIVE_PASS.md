# Resume handoff

Updated: 2026-10-25. **Pass in progress** (the user's "continue"). Committed checkpoint: GS threads and the 60 fps crash fix. Next: item 1 below.

## Where the work stands

The whole game runs without input (all modes, all 200 story parts, 73 towns; [coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)); what each part was checked on is in [VALIDATION.md](VALIDATION.md). paraLLEl-GS is the only renderer. Settings in the launcher and the in-game menu: HD textures, super-sampling, widescreen, 60 fps battles (all from the next start), integer scaling, window size, fullscreen.

**GS threads (2026-10-25,** [performance plan](../plans/ee/PERFORMANCE_PLAN.md) item 3): the game's thread runs the game, VIF1 and VU1 and checks and copies the GIF data; the main renderer's GS thread names textures and feeds paraLLEl-GS (`gs/gpu/gs_render_thread.*`, `gpu_gs_mirror.*`). With 60 fps the in-between pictures have a thread that runs the blended VIF1 lists and checks their data, and the second renderer has its own GS thread. The game's thread waits for a GS thread only at a flip, scanout, readback or local-to-host read. Game state and pictures unchanged (state hash on all 6,534 fields, snapshots pixel for pixel; also with 60 fps on against off).

**The 60 fps crash (fixed 2026-10-25):** unpaced runs with 60 fps crashed with heap corruption within seconds (also before this pass; the user may have met it in play). Cause found with AddressSanitizer: Granite dropped used staging blocks larger than its pool's size, so paraLLEl-GS freed them without the device lock while the other renderer's thread used the same lists. Fix: `tools/graphics/gpu/granite_retired_blocks.patch` (listed in `config/parallel_gs.lock.json`; `fetch_parallel_gs.py` applies it).

**Performance (this PC, Release):** scripted fight headless: plain 30.4 s (33.1 before), HD 33.4 s (50.0 before). In the window, CPU vs CPU at 16x: HD with 60 fps holds 30 flips per second (22 to 27 before), the game's thread working 12 to 24 ms of 33. 60 fps without HD shows 2,067 of 2,181 in-between pictures (7.6 ms each). **With HD the in-between pictures are not ready in time:** 15 to 18 ms each in that battle (7.5 ms in the gate's fight) against about 10 to 14 ms available after a flip's picture; the second renderer's texture naming (hashes, palette ranges, upload copies, name strings) is most of it.

**Not seen by the user yet** (built, gate passed): the GS threads and the 60 fps fix; HD textures as a drop-in and the 60 fps rework of 2026-10-21; the play test fixes of 2026-10-21; the sound fix; fast forward; saving; special moves; the films with and without a skip; the launcher's screens; the camera in fights.

## Next (the user, 2026-10-25: use the cores, RAM and VRAM; native code where it pays)

1. **HD textures with 60 fps** ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md), step 7): the second renderer takes the main renderer's names for the same frame instead of naming again (fallback to naming when the state differs). Measure with the 60 fps summary line's time per in-between picture, in the window (`window_fight`-style run: `BT2_START_BATTLE=3,8,1`, `BT2_VIEWER_STATS`, `BT2_VIEWER_CLOSE`, 2,400 flips). Then the camera-cut threshold (165 cuts in that battle) and the newer frame's own things (hit flashes).
2. **Native texture streaming** (step 8): an index at start, files read ahead into RAM on a background thread, GPU images made off the GS threads, a larger VRAM budget; no replacement drawn late.
3. **Colour-matched variants for the fighters' relit bodies** (step 9); the lighting stays the game's. The user agreed to this order and that the lighting is not replaced.

**After this:** FSR 1 scaling with sharpening and an edge filter ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)); the button art choice; the final battle sweep; the runtime generalization plan's groups 4 to 6 at the very end.

**Not compared with the original yet:** memory card replies, GS readback pixels, 3D frames, VU1 memory. The README's system requirements are estimates from this PC's measurements.

**References checked out locally** (ignored `work/references/`): Tenkaichi3Decomp at `aec1f8a` (`t3d/`: `docs/`, `port/`, `scripts/` only, never its decompiled `src/`), BT3-Recomp at `e22a34b` (`bt3r/`). Their port notes on texture packs and in-between pictures: `t3d/docs/port/README.md`.

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (in `build\probe\Release`), `data` holds the disc image and `data\textures` the texture pack (pack 2 is linked there on this PC); `saves` holds the cards; the settings stay in `user\settings`, the last run's messages in `user\last-run.log`.
- Helper scripts in `work\scripts\` (`work\WORKING_WITHOUT_AI.md`): `play.cmd`, `discover.cmd` (a stop at untranslated code: maps it and rebuilds), `rebuild.cmd`, `gate.cmd`, `show-log.cmd`.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (builds, unit tests, Python suites, probe runs with the original comparisons, the Release battle run against `5a4960667d4a3dff`).
- **Without input:** `BT2_START_BATTLE`, `BT2_START_SCREEN`, `BT2_START_EPISODE` with `--pad-script ""`; sweeps: `battle_sweep.py`, `story_sweep.py`, `screen_run.py` (at most 6 jobs).
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>;…"`. **Audio:** `BT2_AUDIO_WAV=<absolute path>`. **Did the game change:** `BT2_STATE_HASH` on both runs, then `compare_state_hash.py`.
- **Picture settings:** `BT2_HD_TEXTURES`, `BT2_SUPER_SAMPLING`, `BT2_GAME_PATCHES=widescreen`, `BT2_SIXTY_FPS=1` (`check`: blend 1, must equal the main picture; `replay`). **Texture coverage:** `BT2_TEXTURE_NAMES=<file>`, then `tools/graphics/textures/texture_name_check.py <file> <pack>`. **60 fps study:** `BT2_VIF1_DUMP`, `BT2_VIF1_REPLACE`, `BT2_FIGHT_UNCAPPED`. **Cost:** `BT2_VIEWER_STATS`, `BT2_GPU_TIMESTAMPS`, `BT2_SAMPLE_PROFILE` ([TOOLS.md](../guides/TOOLS.md)).
- **Build times:** a runtime header about 16 s; everything about 6 minutes after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`.

## Rules learned the hard way

- **Build the player programs before handing a build over;** a build fails to link while a game window is open.
- **Runs with a window appear on the user's screen** (`BT2_VIEWER_CLOSE=1`); runs with `--sound` play on the user's speakers.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **Threading changes are checked unpaced (headless) too:** the 60 fps crash never showed in the window. Tools that found it: an AddressSanitizer build (configured on demand in `build/asan`, RelWithDebInfo with `/fsanitize=address`, configured from PowerShell; run through `tools/build/msvc.py` so the runtime DLL is found), the Vulkan SDK's validation layer (its thread checks hid the race), the Debug probe (library assertions now go to the error output).
- **After a change to `prepare_runtime.py`:** run it (and `generate_interpreter.py --reuse`), then build.
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
- **Commits carry no assistant attribution.**
