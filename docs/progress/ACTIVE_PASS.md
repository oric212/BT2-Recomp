# Resume handoff

Updated: 2026-11-09. Earlier work: [November history](2026-11.md).

## Current work: 60 fps as the user plays it

The user played a story fight with 60 fps at their settings: flicker in fast movement, input that feels late, motion that looks uneven. Details and numbers: [60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), "The user's play report".

- **Fixed (not yet seen by the user in play):** the game's own GS packets (VIF1 DIRECT blocks: a Super Saiyan aura, dust, the sky, the radar) stayed where the newer frame has them while the fighters were drawn halfway, so the aura covered the body in every in-between picture. They are now blended (`runtime/vu/gs_packet_blend.*`). Checked in pictures with and without HD (`work/frames/sixty-flicker/`, `work/frames/sixty-wide/`); 0.31 ms of processor time per in-between picture.

- **Feel measured (later the same day):** 60 fps adds no delay (30 fps fights measure the same, about 25 ms flip to screen). The user's display runs at 144 Hz, where 59.94 pictures a second cannot be even (2 or 3 refreshes each): **the setting "Variable refresh rate"** (Display, off by default) presents pictures at once for VRR displays; the user's display has VRR. Not yet seen by the user.

- **Even 60 fps (later):** late flips came from the game's core sleeping between bursts (the pacer now spins), and the in-between picture now goes out from its own thread exactly a field after the flip's: 97% of picture intervals within 1 ms of a field with VRR, flip to screen 11 ms instead of 25. Not yet seen by the user.

- **GPU completion (found at the end of the last pass):** a VRR panel shows a picture when its GPU work is done; main pictures were done 8.5 ms after their present, in-between pictures 1.4 ms, so the panel showed them about 10 and 24 ms apart, the steady judder the user felt.
- **Built:** the presenter waits for each picture's GPU work; a flip's picture goes out a steady time after its handover that covers 99% of recent ones, the in-between picture a field after it. 97.1% of intervals within 1 ms of a field as the panel shows them, all in-between pictures shown; a main picture on the panel 16.6 ms after its flip (10.4 before, unevenly). **The user: "it feels smoother"** (their recording: 97.2% within 1 ms in the 60 fps stretches). Pictures of fast loading or fast forward are no longer learned from (they held the menus after a load).

- **Latency (later):** the native fighters pass kept the in-between renderer on the GPU's only graphics queue, ahead of every main picture's work. Off by default now (the user's decision): a main picture on the panel 6.9 ms after its flip instead of 16.7, the in-between renderer's GS thread 1 ms less a frame, the same pictures shown. Fast forward: confirmed working by the user. The user: "the match did feel smooth".

Open, in order:

1. Done: the user's match with the fighters pass off "did feel smooth" (7.5 ms flip to panel, 96.9% within 1 ms); in-between pictures are no longer drawn while the game runs free.
2. Frames without an in-between picture by design, if the user still feels them: looked at 2026-11-03 ([60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), "Frames without an in-between picture"): cuts, scene changes and lists that change by thousands of unpacks; 7 of 43 in 3,000 flips are just over the pairing limit, which a cheaper alignment could take.
3. Done 2026-11-03: the bones "left at the newer pose" are 95% data VU1 never reads (proven by pictures); the rounding helpers 30% faster, bit-identical; the pacer spins only the last 6 ms of a wait (the game's thread 0.67 of a core instead of 0.98). Online battle ([plan](../plans/online/ONLINE_BATTLE_PLAN.md), the user's choices: each player's own patches and save, the game's own Dueling screens entered fresh on both machines, every fighter unlocked, no split screen, no radars, direct address and port): built: local two-player ("Player 2 controller"), the roster unlock (`BT2_UNLOCK_ROSTER`), each player full screen without radars (`BT2_ONLINE_VIEW`), all measured (the fight's audio identical across settings). Not yet seen by the user: two-player, the icons.
4. Done 2026-11-04: the lockstep transport (`BT2_NET`, `host/net_lockstep.*`, `probe/online_session.*`) and the launcher's "Online battle" screen (Host, Join, the address and port typed, the input delay; a waiting screen with Cancel; Escape leaves a battle). Measured over loopback with two probes at different settings: a played fight with 84 drift checks alike and byte-identical audio; a forced difference stopped both by name ([plan](../plans/online/ONLINE_BATTLE_PLAN.md), "The lockstep transport"). Polished the same day at the user's request: Auto input delay, UPnP, connection codes (8 characters), the waiting screens, each player's side shown outside fights, wait and slow-connection lines, the pacer's cushion after waits (late host pictures at 80 ms with jitter: 97 of 999 to 1), a scripted fight over a simulated 60 ms with byte-identical audio. Then the launcher's online screen simplified (Host, Join with a code, Options), the player's readme and online setup guide (`docs/guides/release/`), and a private release package (`python tools/build/package.py`, `dist/`; [release plan](../plans/launcher/RELEASE_PLAN.md)). **Next: the user's two machines with the package** (UPnP or the port forwarded by hand); the user's decisions: a matchmaking server for a short PIN, rollback. Releases follow BT3-Recomp's model (the user's decision): the compiled runtime ships, the game's data stays on the player's disc; the game's starting memory is now made from the disc (`ee/guest_image.*`, byte-identical, gate passed). Then every console capture replaced natively (`EeRuntime::bootState`, the kernel's syscall table view; the runtime's command line `STOP_REPORT DISC_IMAGE`): the package holds nothing of the game or the console, checked from a clean folder; not published (the user decides when).

## GPU compatibility (2026-11-09)

- The fade into a fight striped on the user's RX 9070 XT: paraLLEl-GS's 24-bit upload (`vram_copy.comp`) now reads and writes single bytes ([GPU compatibility](../plans/graphics/GPU_COMPATIBILITY.md)); verified headless, **not yet seen by the user in play**. Open: HD faces and muscle lines missing on that card.
- The release package rebuilt from this commit (`dist/`, not published).

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (`build\probe\Release`): `data` (disc image; `data\textures` holds the pack in its own subfolder), `saves` (cards), `user\settings`, `user\last-run.log`.
- **The user's settings:** fullscreen at window scale 4 (the 4x scanout), 16x, HD, widescreen, 60 fps, Light depth of field (`BT2_DOF_STRENGTH=25`).

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (the Release battle run against its baseline in `tools/gate/battle_run.py`).
- **Feel in fullscreen at the user's settings:** the story fight with `--viewer --frames 3000`, `BT2_VIEWER_FULLSCREEN=1 BT2_VIEWER_SCALE=4 BT2_VIEWER_CLOSE=1 BT2_LATENCY_LOG=<file>` and the picture settings below (on the user's screen); `latency_report.py`.
- **The CPU fight at the 60 fps settings, headless:** `BT2_START_BATTLE=0,1,0 BT2_SUPER_SAMPLING=16 BT2_SCANOUT_QUAD=1 BT2_HD_TEXTURES="<pack subfolder>" BT2_GAME_PATCHES=widescreen BT2_SIXTY_FPS=1 BT2_DOF_STRENGTH=25` with `run_startup_probe.py --config Release --frames 2000 --pad-script ""`; the run's 60 fps line splits the in-between thread's time. A story fight: `BT2_START_EPISODE=0,0,marker:14`.
- **Online over loopback:** the host with `BT2_NET=host:7411`, then the joiner with `BT2_NET=join:127.0.0.1:7411`, each `run_startup_probe.py --config Release` with its own `--report` and `--pad-script` (the local player's input); the last line of each log counts the drift checks found alike. Two runs at once are needed here (the one exception besides sweeps).
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
