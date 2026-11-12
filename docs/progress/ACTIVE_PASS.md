# Resume handoff

Updated: 2026-11-12. Finished work: [November history](2026-11.md).

## State

No pass is open. Last done (2026-11-12): the [PCSX2 comparison](../plans/validation/PCSX2_COMPARISON_PLAN.md), steps 1 and 2: sessions with resumable checkpoints (`tools/pcsx2_compare/`); three code faults fixed (SQRT.S's register and rounding, MOVN/MOVZ's width); the fight frame's logic and draw steps and a full versus battle equal to PCSX2; the battle baseline rerecorded; the release package rebuilt (`dist/BT2-Recomp-8227c74`).

**Next:** that plan's step 3 (consecutive frames: one session of the logic and draw steps at successive frames, each replayed), then step 9 (input lag and latency), step 5 (loaded data). Then the [implementation plan](../project/IMPLEMENTATION_PLAN.md)'s order.

## Waiting for the user

- **Open reports, documented only:** the destroyed Namek still flickers at 60 fps after the lava fix: what, when, with 60 fps only? ([60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), "Still present for the user"); Frieza over the ocean with HD: a doubled outline, broken lines, blocky water, the user suspects the HD streaming ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md), picture in `work/frames/hd-textures/`); characters under water not in HD (same plan); a texture glitch in the first HD fight.
- **In play, with the release package:** online on two PCs (lock-on from "FIGHT!", the connect screen, pausing; guide `docs/guides/release/ONLINE_SETUP.md`); Low input lag (60 fps fights or quicker 30 fps ones); on the RX 9070 XT the HD faces and the fade; Variable refresh rate, beam struggles by buttons, the second controller; distant fighters' sharpness (a PCSX2 picture of the expected look).
- **Their decisions:** responsiveness step 7; a matchmaking server, rollback, publishing.

## How the user runs things

- **Game folders** next to `bt2-game.exe` (`build\probe\Release`): `data` (disc image; the HD pack in its own subfolder of `data\textures`), `saves` (one folder per card), `user\settings`, `user\last-run.log` (names the card in slot 1; the user's own runs on this PC write `D:\Cs\Cpp\BT2-Recomp\user\last-run.log`, which says why 60 fps or a session stopped).
- **Their settings:** fullscreen at window scale 4, 16x, HD, widescreen, 60 fps, light depth of field.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>`.
- **The CPU fight headless:** `BT2_START_BATTLE=0,1,0` with `run_startup_probe.py --config Release --frames 2000 --pad-script ""`; with HD `BT2_HD_TEXTURES=<absolute path of the pack subfolder>`; a story fight `BT2_START_EPISODE=0,0,marker:14`.
- **Saving:** `BT2_CARD_FOLDER=<card folder>`; the title's New Game with Yes to "create a save file" (pad script: the gate's starts, then `2500:cross,2650:left,2700:cross`). Original saves: `tools/saves/card_image.py` reads PCSX2 card images, `import_save.py` `.cbs` files.
- **Online over loopback:** `BT2_NET=host:7411` and `BT2_NET=join:127.0.0.1:7411`, each its own `--report` and `--pad-script` (two runs at once are allowed here).
- **PCSX2 comparison sessions:** `tools/pcsx2_compare/session.py` with the PCSX2 copy, BIOS and card copy under `work/pcsx2-capture/ai-think-before-200/` (`package/pcsx2-qt.exe`, `profile/PCSX2/bios/scph39001.bin`, `profile/PCSX2/memcards/Mcd001.ps2`); about 200 s to the demo battle, which `--site %101A60:1:1:1 --edit %101A60:a0=0,a2=0,a3=1` makes a full versus battle; end with `--until <site>`; each checkpoint 32 MB under `work/pcsx2-compare/`; then `compare_calls.py --session <name>` ([TOOLS.md](../guides/TOOLS.md), "PCSX2 comparison sessions").
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>.ppm;…"`; 60 fps: `BT2_SIXTY_FPS_SNAPSHOT`, `BT2_SIXTY_FPS=check`. Other tools: [TOOLS.md](../guides/TOOLS.md).

## Rules learned the hard way

- **Rebuild Release (all) before handing a build over;** a build fails to link while a game window is open.
- **Measure at the user's settings and content;** headless for processor work, the window for what the player sees.
- **60 fps pictures are judged by looking;** the difference score only picks pictures.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **The graphics device's lock couples every thread:** change the frame only at the flip, a readback or a picture shown.
- **Paths given to the probe are absolute** (the HD pack, snapshots, cards); the HD pack's path is its subfolder.
- **The Release probe links incrementally:** after editing a `bt2-ee-runtime` source, delete `build/probe/Release/bt2-entry-probe.ilk` before building (a target-only build once kept the old code), and check a new string is in the executable.
- **Shell heredocs corrupt backslashes;** write such files with the file tools.
- **PCSX2 sessions:** the software renderer (the null one stops at an assertion dialog in a versus battle); a stub's data on a page apart from its code (PCSX2 write-protects compiled code); ask only for checkpoints that will come, or end with `--until`.
