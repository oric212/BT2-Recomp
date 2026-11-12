# Resume handoff

Updated: 2026-11-12. Finished work: [November history](2026-11.md).

## State

No pass is open. Last done (2026-11-12): the [PCSX2 comparison](../plans/validation/PCSX2_COMPARISON_PLAN.md), steps 1 to 3: sessions with resumable checkpoints (`tools/pcsx2_compare/`); three code faults fixed (SQRT.S's register and rounding, MOVN/MOVZ's width); the fight frame's logic and draw steps, a full versus battle and 300 consecutive frames (session `trajectory`, every replayable call of the fight loop) equal to PCSX2; checkpoints stored as changed pages; the replay loads VU0's R register where the runtime keeps it. The release package (`dist/BT2-Recomp-8227c74`) is unchanged by this pass (tools and the replay only).

Also done (the same day): step 9's first part, the game's own input delay: logging sites that do not wait (`session.py --log`, `--set`), `BT2_EE_CALL_SUMS` for the port, `input_delay.py`; a press at fight frame 480 changes the lists in its frame and VU1 a frame later in both programs (sessions `delay-base`, `delay-x480`, `delay-u480`; port runs in `work/latency/`).

Then step 9's second part: `press_to_screen.py` (unpauses timed from `SendInput` to the first changed picture, windowed): port 104 ms median with Low input lag, 111 ms at 60 fps, 133 ms without; PCSX2 108 ms (`work/latency/*.json`).

Then step 5, loaded data, at a fight's first frame: `BT2_EE_CALL_RAM` (the port's RAM before a call) and `loaded_data.py` (ranges against `config/pcsx2_compare_regions.json`); sessions `loaded` (the versus battle, the comparison that counts) and `loaded-demo` (the plain title demo: another draw, not comparable). The loaded fighters and stage equal; 23 KB of globals differ by route, not yet explained one by one; sticks at rest `0x80` in the port against PCSX2's `0x7F`, the fight the same with either (lists and VU1 in 1,427 frames).

Then step 6, VU1: the kick `0x28B090` replayed through the port's VIF1 and VU1 with VIF1's registers (`hardware.bin`, read by the stub) and VU1's (`vu1-regs.bin`, found in PCSX2's `VURegs` by `vu_registers.py`); sessions `vif1d` (microVU) and `vif1f` (`--vu1-interpreter`), compared at `@102EE0` (the chain finished). The I-bit fault fixed (interpreter, translator, hand-written clipper; `vu1_native.cpp` regenerated from `work/perf/vu1-census`); VU1's data memory equal after all five chains; battle baseline rerecorded. Sessions `vif1`, `vif1b`, `vif1c`, `vif1e` were steps on the way (no hardware or VU1 registers, or snapshots before the chain ended).

Then step 7 started: save states through PINE at checkpoints (`--save-state`), `gs_memory.py` (GS.bin version 9, GS memory at 425), the replay's GIF setup and GS memory out, `gs_compare.py`; session `gs3` (fight frame 300: kick `@102ED8`, settled at the frame's end `@102F08`). Textures exact, Z 1 word, the frame within ±1 to 2 levels but 1,047 pixels.

**Next:** step 7 at fight frames with the fighters, super attacks and effects (frame 300 is the intro's stage pan); the globals by route (a port run through PCSX2's route would remove them). Fullscreen and press to sound if the user wants them. Then the [implementation plan](../project/IMPLEMENTATION_PLAN.md)'s order.

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
- **Input delay:** `session.py ... --set %101A60:1:a0=0,a2=0,a3=1 --set %101AB0:1:a2=0 --set %101AF8:1:a3=0 --log "@102EE0:700,word=3B0C2C[,press=480/2/3B0C2C/4000]" --log @102F08:700,word=3B0A84,word=3B0A80,sum=430000-457000,sum=1100C000-11010000,sum=530000-557000 --until @102F08` (about 4 minutes); the port: `BT2_START_BATTLE=8,120,9,23 BT2_START_BATTLE_PLAYER=1 BT2_EE_CALL_SUMS="<file>:102ee8,102ec8:3b0c2c:430000-457000:1100c000-11010000:530000-557000"`, frame 480's read at field 1489; then `input_delay.py`. Presses during the intro (before frame 471) do nothing.
- **PCSX2 comparison sessions:** `tools/pcsx2_compare/session.py` with the PCSX2 copy, BIOS and card copy under `work/pcsx2-capture/ai-think-before-200/` (`package/pcsx2-qt.exe`, `profile/PCSX2/bios/scph39001.bin`, `profile/PCSX2/memcards/Mcd001.ps2`); about 200 s to the demo battle, which `--site %101A60:1:1:1 --edit %101A60:a0=0,a2=0,a3=1` makes a full versus battle; end with `--until <site>`; each checkpoint 32 MB under `work/pcsx2-compare/`; then `compare_calls.py --session <name>` ([TOOLS.md](../guides/TOOLS.md), "PCSX2 comparison sessions").
- **VU1 against PCSX2:** `session.py ... --site "@102ED8:300:3:300:after" --site "@102EE0:300:3:300" --until "@102EE0"` (add `--vu1-interpreter` for PCSX2's VU1 interpreter); `compare_calls.py --session <name>` replays each kick and lists VU1's data memory; compare against the `@102EE0` checkpoint when PCSX2's chain may not have ended at "after" (D1_CHCR in `hardware.bin` word 25).
- **GS memory against PCSX2:** `session.py ... --site "@102ED8:300:1:1" --site "@102F08:300:1:1" --save-state "@102ED8" --save-state "@102F08" --until "@102F08"`, then `gs_compare.py --session <name> --call "@102ED8-300-before" --settled "@102F08-300-before"` (about 1 minute; pictures in `work/frames/pcsx2-compare/`). Compare at the frame's end: PCSX2's GS may still be drawing just after the kick.
- **Loaded data:** the port's RAM with `BT2_EE_CALL_RAM=<absolute file>:102ef8:1` and the starter's battle (`BT2_START_BATTLE=8,120,9,23 BT2_START_BATTLE_PLAYER=1`, field 531); `loaded_data.py --pcsx2 work/pcsx2-compare/loaded/step-b-1-before --port <file>`. In PowerShell an empty pad script is `"--pad-script="`.
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
