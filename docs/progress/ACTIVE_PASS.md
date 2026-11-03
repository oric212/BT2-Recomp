# Resume handoff

Updated: 2026-11-03. Earlier work: [November history](2026-11.md).

## Current work: 60 fps as the user plays it

The user played a story fight with 60 fps at their settings: flicker in fast movement, input that feels late, motion that looks uneven. Details and numbers: [60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), "The user's play report".

- **Fixed (not yet seen by the user in play):** the game's own GS packets (VIF1 DIRECT blocks: a Super Saiyan aura, dust, the sky, the radar) stayed where the newer frame has them while the fighters were drawn halfway, so the aura covered the body in every in-between picture. They are now blended (`runtime/vu/gs_packet_blend.*`). Checked in pictures with and without HD (`work/frames/sixty-flicker/`, `work/frames/sixty-wide/`); 0.31 ms of processor time per in-between picture.

- **Feel measured (later the same day):** 60 fps adds no delay (30 fps fights measure the same, about 25 ms flip to screen). The user's display runs at 144 Hz, where 59.94 pictures a second cannot be even (2 or 3 refreshes each): **the setting "Variable refresh rate"** (Display, off by default) presents pictures at once for VRR displays; the user's display has VRR. Not yet seen by the user.

- **Even 60 fps (later):** late flips came from the game's core sleeping between bursts (the pacer now spins), and the in-between picture now goes out from its own thread exactly a field after the flip's: 97% of picture intervals within 1 ms of a field with VRR, flip to screen 11 ms instead of 25. Not yet seen by the user.

Open, in order:

1. The user's look in play with "Variable refresh rate" on: the aura fix and the even pictures.
2. 40 of 2,826 in-between pictures left out (1.4%): the reason (ready after the next flip's picture, or not keeping up).
3. Bones left at the newer pose: 10% a picture, most without a counterpart.
4. Load: the pacer's spin keeps one core busy while the game waits (the user's go-ahead for more hardware, 2026-11-03); the AI pass's rounding helpers (about 8% of the game thread); then [native fight rendering](../plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md)'s remaining load work.

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (`build\probe\Release`): `data` (disc image; `data\textures` holds the pack in its own subfolder), `saves` (cards), `user\settings`, `user\last-run.log`.
- **The user's settings:** fullscreen at window scale 4 (the 4x scanout), 16x, HD, widescreen, 60 fps, Light depth of field (`BT2_DOF_STRENGTH=25`).

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (the Release battle run against `7f996c1524a22bea`).
- **Feel in fullscreen at the user's settings:** the story fight with `--viewer --frames 3000`, `BT2_VIEWER_FULLSCREEN=1 BT2_VIEWER_SCALE=4 BT2_VIEWER_CLOSE=1 BT2_LATENCY_LOG=<file>` and the picture settings below (on the user's screen); `latency_report.py`.
- **The CPU fight at the 60 fps settings, headless:** `BT2_START_BATTLE=0,1,0 BT2_SUPER_SAMPLING=16 BT2_SCANOUT_QUAD=1 BT2_HD_TEXTURES="<pack subfolder>" BT2_GAME_PATCHES=widescreen BT2_SIXTY_FPS=1 BT2_DOF_STRENGTH=25` with `run_startup_probe.py --config Release --frames 2000 --pad-script ""`; the run's 60 fps line splits the in-between thread's time. A story fight: `BT2_START_EPISODE=0,0,marker:14`.
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>.ppm;…"`; in-between: `BT2_SIXTY_FPS_SNAPSHOT` (flip F is the picture between main F and F + 1); `BT2_SIXTY_FPS=check` must equal main F + 1 exactly.
- **Tools** (GS and VIF1 dumps, sampling, frame timing): [TOOLS.md](../guides/TOOLS.md); the in-between thread is `BT2_SAMPLE_GS_THREADS`'s `-1`.

## Rules learned the hard way

- **Rebuild Release (all) before handing a build over,** so the program the user plays is the committed source; a build fails to link while a game window is open.
- **Measure at the user's settings and content:** a CPU-vs-CPU fight in a window hid the user's 60 fps drop. Compare headless for processor work, the window for what the player sees, each against the committed build in the same conditions.
- **60 fps pictures are judged by looking:** the score of pixels outside both main pictures also rises when something correctly moves halfway (effects since 2026-11-03); use it to pick pictures, not to decide.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **The graphics device's lock couples every thread:** change the frame only at the flip, a readback or a picture shown; pause GS threads rather than drain them. Window runs show this; headless runs do not.
- **The HD pack's path is its subfolder** (`data\textures\SLUS-21441-texture pack 2`); the parent loads nothing ("0 in the pack").
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
