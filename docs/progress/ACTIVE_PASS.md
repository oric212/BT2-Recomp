# Resume handoff

Updated: 2026-11-09. Finished work: [November history](2026-11.md).

## State

No pass is open. Next after online: the user's report that a DualSense feels less responsive than an Xbox Series controller (not yet looked into). Done today: saves verified (byte-identical to the original's in PCSX2, written to the launcher's card only), the HD faces fixed (lit-variant choice), the RX 9070 XT's striped fade worked around, the online setup simplified. Next: the user's play, then the final validation ([PROG.md](../PROG.md), "Next").

## Waiting for the user

- **Online on two PCs** with the release package (`python tools/build/package.py`, `dist/`; guide `docs/guides/release/ONLINE_SETUP.md`): the reply code and hole punching (no port to open), a whole match and a second one. Their decisions: a matchmaking server (short PIN, no open port), rollback, publishing.
- **In play:** the HD faces and the fade on the RX 9070 XT, the 60 fps aura fix, Variable refresh rate, beam struggles by buttons, the HD outline, the second controller, the icons. A texture glitch in the first HD fight is reported but not reproduced headless.

## How the user runs things

- **Game folders** next to `bt2-game.exe` (`build\probe\Release`): `data` (disc image; the HD pack in its own subfolder of `data\textures`), `saves` (one folder per card), `user\settings`, `user\last-run.log` (names the card in slot 1).
- **Their settings:** fullscreen at window scale 4, 16x, HD, widescreen, 60 fps, light depth of field.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>`.
- **The CPU fight headless:** `BT2_START_BATTLE=0,1,0` with `run_startup_probe.py --config Release --frames 2000 --pad-script ""`; with HD `BT2_HD_TEXTURES=<absolute path of the pack subfolder>`; a story fight `BT2_START_EPISODE=0,0,marker:14`.
- **Saving:** `BT2_CARD_FOLDER=<card folder>`; the title's New Game with Yes to "create a save file" (pad script: the gate's starts, then `2500:cross,2650:left,2700:cross`). Original saves: `tools/saves/card_image.py` reads PCSX2 card images, `import_save.py` `.cbs` files.
- **Online over loopback:** `BT2_NET=host:7411` and `BT2_NET=join:127.0.0.1:7411`, each its own `--report` and `--pad-script` (two runs at once are allowed here).
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>.ppm;…"`; 60 fps: `BT2_SIXTY_FPS_SNAPSHOT`, `BT2_SIXTY_FPS=check`. Other tools: [TOOLS.md](../guides/TOOLS.md).

## Rules learned the hard way

- **Rebuild Release (all) before handing a build over;** a build fails to link while a game window is open.
- **Measure at the user's settings and content;** headless for processor work, the window for what the player sees.
- **60 fps pictures are judged by looking;** the difference score only picks pictures.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **The graphics device's lock couples every thread:** change the frame only at the flip, a readback or a picture shown.
- **Paths given to the probe are absolute** (the HD pack, snapshots, cards); the HD pack's path is its subfolder.
- **Shell heredocs corrupt backslashes;** write such files with the file tools.
