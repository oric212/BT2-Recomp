# Resume handoff

Updated: 2026-10-27. The HD outline is built: the native pass `ink-edges` inside paraLLEl-GS draws the ink outline's edge buffer per sample ([hybrid rendering plan](../plans/graphics/HYBRID_RENDERING_PLAN.md) step 2). At 1x GS memory is the renderer's exactly; at 16x the band follows the geometry at the original's thickness. It stays off by default until the user has looked at it (`work/frames/ink-compare-1000.png` and `-1200.png`: 1x, today's 16x, the native pass at 16x; in the game `BT2_NATIVE_PASSES=ink-edges`). Next: the user's look, then item 1 below.

## Where the work stands

The whole game runs without input (all modes, all 200 story parts, 73 towns; [coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)); what each part was checked on is in [VALIDATION.md](VALIDATION.md). paraLLEl-GS renders every frame; native passes inside it are planned (hybrid plan). Settings in the launcher and the in-game menu: HD textures, super-sampling, widescreen, 60 fps battles (from the next start), integer scaling, window size, fullscreen, button art.

**Threads:** the game's thread runs the game, VIF1 and VU1 and checks the GIF data; the main renderer's GS thread names textures and feeds paraLLEl-GS; with 60 fps an in-between thread runs the blended VIF1 lists and a second renderer has its own GS thread, its GPU work on the async compute queue beside the main renderer's (`parallel_gs_compute_queue.patch`, `granite_submit_waiting.patch`; its scanouts on the graphics queue); the window's pictures are presented on a thread of their own through a DXGI flip-model swapchain (`granite_dxgi_copy_target.patch`), off the graphics device's lock ([performance plan](../plans/ee/PERFORMANCE_PLAN.md) items 3, 4d).

**Image quality at 16x** ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md) steps 3 to 5): 16 samples per game pixel (2,048 x 1,792 for the fight's 512 x 448 frame); a window taller than 896 lines shows one sample per pixel (2,560 x 1,792 after the 640-wide stretch, "4x native"), a smaller one averages four (1,280 x 896). HD art takes texture coordinates per sample; the game's effect passes (depth of field, the pause's blurred scene) read their buffers with their samples (paraLLEl-GS's super-sampled textures); the ink outline is the game's own band per game pixel, or with the native pass `ink-edges` a band of the same thickness computed per sample; the last game line is hidden at high resolution and region clamps stop at a texture's size (reads past what the game drew).

**60 fps** ([60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md)): rigid transforms and cameras blended between two fight frames (`vu/vif1_blend.*`); fights shown a field later so the in-between pictures have a whole field; no in-between picture across a camera cut or a change of how the scene is drawn (`sameScene`, the teleport's afterimage). Check mode exact with and without HD at 16x and 4x.

**Performance (this PC: i9-10900K, RX 6800 XT; window, CPU vs CPU `BT2_START_BATTLE=0,1,0`, 16x, 4x scanout, HD, widescreen, 60 fps):** 29.97 flips per second except the Angry Kamehameha at flips 3,280 to 3,480 (29.17 with the in-between renderer on the compute queue, was 28.23; the game's wait 6.2 ms a flip there, was 10.3): still GPU-bound there (about 70 screens of blended dust a frame, drawn by both renderers; 8x holds it). Over the rest of the fight the main GS thread works 12.7 ms a flip, the game's thread waits 0.6 ms for it; GPU about 9.5 ms a frame for the main renderer (14 in the super attack). Most GPU time is fixed per frame (shading at 1x 3.7 ms, 16x 7.5).

**Not seen by the user yet** (built, gate passed): the HD outline (off by default, pictures above); the in-between renderer on the async compute queue (the super attack's frame rate); the DXGI presentation and the stretching fix; effects at the super-sampled resolution (`work/frames/effects-141212-*.png`); the 4x scanout's sharp HD art, fullscreen, the menus' greyed entries, the random fighters, the Map Select music, 60 fps fights shown a field later; HD with 60 fps; the GS threads; earlier items listed in the October history.

## Next

1. **[Hybrid rendering](../plans/graphics/HYBRID_RENDERING_PLAN.md)** (the user's decision of 2026-10-26): the groundwork and the HD outline are built (native passes at paraLLEl-GS's render passes in `runtime/gs/gpu/native/`, switched by `BT2_NATIVE_PASSES`, compared at 1x with `BT2_GS_MEMORY_HASHES`). After the user's look at the outline: on by default, with a player setting. Then the super attacks' blended layers, after measuring their cost (step 3): they are what still holds the super attack under 30 at 16x (29.17 with the async compute queue; [performance plan](../plans/ee/PERFORMANCE_PLAN.md) item 4d), so the user's flat 60 with headroom depends on them. The native passes' own GPU time is not yet in `BT2_GPU_TIMESTAMPS`; it needs a stage of its own before step 3's measurements.
2. **Then:** FSR 1 scaling with sharpening and an edge filter; the final battle sweep; the runtime generalization plan's groups 4 to 6 at the very end.

**Open reports from the user:** the frame rate in super attacks (improved, not yet a flat 30 at 16x; item 1). Earlier, not reproduced or awaiting the user's look: the same random music twice; "a fuzzy look in some areas" (the soft areas found are originals: the Map Select preview, a film).

**Not compared with the original yet:** memory card replies, GS readback pixels, 3D frames, VU1 memory. The README's system requirements are estimates from this PC's measurements.

**References checked out locally** (ignored `work/references/`): Tenkaichi3Decomp at `aec1f8a` (`t3d/`: `docs/`, `port/`, `scripts/` only, never its decompiled `src/`), BT3-Recomp at `e22a34b` (`bt3r/`; its native post-process steps in `docs/NATIVE-RENDER-SEAM.md` and `docs/SEAM-POSTCHAIN.md`, read for the approach only).

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (in `build\probe\Release`), `data` holds the disc image and `data\textures` the texture pack (pack 2 is linked there on this PC); `saves` holds the cards; the settings stay in `user\settings`, the last run's messages in `user\last-run.log`.
- Helper scripts in `work\scripts\` (`work\WORKING_WITHOUT_AI.md`): `play.cmd`, `discover.cmd` (a stop at untranslated code: maps it and rebuilds), `rebuild.cmd`, `gate.cmd`, `show-log.cmd`.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (builds, unit tests, Python suites, probe runs with the original comparisons, the Release battle run against `94b27c5bb9a49166`).
- **Without input:** `BT2_START_BATTLE`, `BT2_START_SCREEN`, `BT2_START_EPISODE` with `--pad-script ""`; sweeps: `battle_sweep.py`, `story_sweep.py`, `screen_run.py` (at most 6 jobs). Runs are repeatable: the same flip shows the same picture in every run (with HD textures, 60 fps in-between pictures can differ between runs: the replacements stream in with timing).
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>;…"` (`BT2_SCANOUT_QUAD=1` for 4x without a window); 60 fps: `BT2_SIXTY_FPS_SNAPSHOT`. **Audio:** `BT2_AUDIO_WAV`. **Did the game change:** `BT2_STATE_HASH`, then `compare_state_hash.py`.
- **Picture settings:** `BT2_HD_TEXTURES`, `BT2_SUPER_SAMPLING`, `BT2_SUPER_SAMPLED_TEXTURES=0` (compare), `BT2_GAME_PATCHES=widescreen`, `BT2_SIXTY_FPS=1` (`check`, `replay`). **Texture coverage:** `BT2_TEXTURE_NAMES`, `BT2_TEXTURE_DUMP`, `BT2_HD_SHOW_MISSING`.
- **GS passes:** `BT2_GIF_DUMP=<flip>:<flips>:<file>` and `tools/graphics/gpu/gif_dump_draws.py` (a flip's draws with their state and where each ends); `BT2_GS_PEEK` with `BT2_GS_PEEK_BUFFERS` (buffers and their alpha between draws).
- **Cost:** in the window `BT2_VIEWER_STATS=1` with `BT2_FLIP_TIMES=<file>` (each flip's time, the wait for the GS threads, their work); `BT2_GPU_TIMESTAMPS` (GPU time by stage; for one stretch of a fight, two runs to its start and end and the difference); `BT2_SAMPLE_GS_THREADS`, `BT2_SAMPLE_PROFILE` (the same subtraction for a stretch) ([TOOLS.md](../guides/TOOLS.md)).
- **paraLLEl-GS shaders:** changed shaders are embedded as SPIR-V built with the Vulkan SDK's glslc (`--target-env=vulkan1.1 -O`, flags in each header's comment; the ubershader's variants `FEEDBACK_COLOR`, `FEEDBACK_DEPTH`); a patch of the checkout goes into `tools/graphics/gpu/` and `config/parallel_gs.lock.json`, and the series is checked by applying it to a clean archive of the pinned revision.
- **Build times:** a runtime header about 16 s; everything about 6 minutes after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`.

## Rules learned the hard way

- **Build the player programs before handing a build over;** a build fails to link while a game window is open.
- **Runs with a window appear on the user's screen** (`BT2_VIEWER_CLOSE=1`); runs with `--sound` play on the user's speakers.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **60 fps pictures are checked as pictures:** `BT2_SIXTY_FPS_SNAPSHOT` at flip F is the picture between main F and main F + 1; `BT2_SIXTY_FPS=check` must equal main F + 1 exactly; score in-between pictures by the pixels outside both main pictures around them.
- **The graphics device's lock couples every thread:** Granite holds it for every command buffer, submission and frame change; change the frame only at the flip, a readback or a picture shown, and pause GS threads rather than drain them around it. Anything slow under it (a Vulkan present waited 2.8 ms) stalls both renderers; count holders by wrapping Granite's `LOCK()` temporarily. A semaphore wait added for a queue belongs to whichever submission comes next on it. Window runs show these effects; headless runs do not.
- **Measure before optimizing the GPU:** an experiment compiled in (texture sampling off, blended primitives off, sample counts 1x to 16x) tells where the time is; noise between identical runs is about 0.8 ms over a 140-flip stretch, so use whole-fight figures where an experiment changes pictures the game reads back.
- **Threading changes are checked unpaced (headless) too:** the 60 fps crash never showed in the window. Tools: an AddressSanitizer build (`build/asan`, RelWithDebInfo with `/fsanitize=address`, configured from PowerShell, run through `tools/build/msvc.py`), the Vulkan SDK's validation layer, the Debug probe.
- **After a change to `prepare_runtime.py`:** run it (and `generate_interpreter.py --reuse`), then build.
- **Synthetic GIF packets in tests set PRMODECONT:** a fresh renderer starts with AC 0, so PRIM's texture, blending and FST bits are ignored until `0x1A` is written with 1.
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
- **Commits carry no assistant attribution.**
