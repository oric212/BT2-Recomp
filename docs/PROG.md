# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). Baseline: verified checkpoint of 2026-10-13.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading (answered natively) |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 1,000 frames in the gate run; 9,000 (the budget) in the long Release run |
| Controller check | ✅ Verified — passed with the native controller service |
| Memory card check | ✅ Verified — no card inserted; the prompt is answered by scripted input |
| Opening movie | 🟢 Operational — the logo movie and the opening movie play to the end (decoded natively, no sound); frames match an independent decoder at spot checks |
| Title screen | 🟢 Operational — the title screen, its option list and the New Game dialogues render and respond to scripted input (the user confirms the pictures) |
| Demo fight (no input) | 🟢 Operational — two CPU-vs-CPU demo fights render in 3D on the GPU renderer and play to their end (the user confirmed the frames; not compared with the original by capture); their sound has not been listened to |
| Menus | 🟢 Operational — main menu (all nine entries scroll), Dueling menu, character selection and map selection render and respond to scripted input (the user confirms the pictures); only the path to a 1P-vs-COM duel has been entered |
| Versus battle | 🟢 Operational — a 1P-vs-COM duel loads and runs with scripted input controlling the fighter (about 4,100 frames to the run's budget); frames confirmed by the user; GPU renderer, with music and sound effects in the run's WAV file, slower than real time, not compared with the original by capture |
| Audio | 🟢 Operational — volumes for the whole mix, music and effects in the launcher and the in-game menu; stream audio (movies, music) and sound effects, including looping and stopped ones, are mixed, written to a WAV file and played on the audio device (`--sound`), which holds the run at the game's speed; the user listened to a WAV of menus and a fight and confirmed it; stream level, channel order and timing match a PCSX2 recording for the boot movies; live sound not yet heard by the user, gaps expected in fights (slower than real time) |
| Saves | 🔧 Incomplete — the user's 100% save on a memory card folder loads (confirmed by the user); writing saves is implemented and unit-tested, not yet tried in the game |

**Measured 2026-10-13:** runtime coverage **2,007 EE functions entered / 1,998 completed** in the verified run (4,936 / 4,927 in the long run through a battle) · static coverage **11,122 EE functions mapped and generated** · verification **18/18 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-13)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 11,122, all compiled into the probe, plus all 494 mapped functions of the `DBZP.BIN` overlay |
| EE functions entered / completed | 2,007 / 1,998 in the gate run (1,000 frames); 4,936 / 4,927 in the long Release run to a battle (4,854 / 4,847 in the idle run of the demo fights) |
| Presented frames / logical VBlanks | Gate run: 1,000 (frame budget) / 1,398. Long Release run (scripted input to a battle): 9,000 (frame budget) / 14,334. In the battle one frame is presented every two fields; whether the original does the same is not checked |
| Native service requests (long run to a battle) | 1,013 disc requests, 48 CRI calls, 76 memory card queries (no IOP code runs) |
| 3D pipeline (long run to a battle) | 8,903 VIF1 display lists: 27.6 million UNPACKs, 65,334 microprogram uploads, 16.8 million VU1 program starts (5.77 billion VU1 instructions), 20.7 million XGKICK packets; 117 million triangles and 3.9 million sprites reach the GS. No VU1 read of Q or the clip flags fell inside a pipeline delay |
| GS readback (battle loading) | 12 local→host transfers, 258,048 quadwords, through the library's store-image sequence |
| Native IPU (long run) | 16.5 million commands: 10.1 million header codes (VDEC), 2.46 million macroblocks (BDEC), 3,387 pictures converted to RGB (logo 151, opening 3,236) |
| Release run time | The fight part of the scripted run takes 32 to 38 ms per presented frame in the window (2026-10-13, after the performance pass; 59 to 73 ms before), so it runs at about the game's speed in this scene if a fight frame is two fields long. Earlier figure: 277 s for the 9,000-frame run to a battle, about 19 frames per second in the battle. One run on one scene, not a general gameplay performance figure |

The gate run ends at its frame budget (1,000 frames, just after the logo movie). The long Release run (`run_startup_probe.py --config Release --frames 9000` with the battle pad script) skips the movies, opens New Game from the title screen (Start opens the option list, Cross confirms; two memory-card dialogues follow), enters Dueling from the main menu, picks 1P VS COM, Single Battle, the characters and the map, and fights until the frame budget ([frames](progress/ACTIVE_PASS.md#frames-to-inspect)). Without input after the movies the game shows its idle cycle instead (demo fight, title, second demo fight). The long runs are Operational, not part of the gate.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code (every mapped function compiled); threads, semaphores, interrupts and priorities as reached; startup states match captures; a busy main thread still receives VBLANKs (logical CPU time), while a worker thread above main runs to its next wait within one field; GS interrupt mask syscalls; VU0 macro-mode code runs inline. Four generator errors are corrected when sources are prepared: the operand of `SQRT.S`, writes to the constant register VF0, the VU0 random number instructions (run natively from the instruction word) and the sign extension of `MFC1` |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🟢 Operational | paraLLEl-GS GPU backend draws every screen reached so far, 2D and 3D, and answers local→host transfers ([readback plan](plans/graphics/GS_READBACK_PLAN.md)); the user confirmed all sampled frames. The CPU GS model draws the 2D screens, validates packets and is the test reference for them; 3D on the CPU renderer (perspective texturing, fog) is an optional extra for the end of the project, so `--gs cpu` and the mirror comparison stop at the first 3D frame |
| GIF / VIF | 🟢 Operational | GIF normal and chain-mode DMA (image data may continue across transfers), DMA CALL/RET tags; VIF1 display lists with tag transfer: state codes, MPG, UNPACK (S and V4 forms, plain write cycle), MSCAL/MSCALF/MSCNT with the double buffer, DIRECT/DIRECTHL, MSKPATH3; VIF1 normal-mode DMA in both directions and the VIF1 FIFO for GS readback. V2/V3 UNPACK, masking, addition modes and code interrupts are refused by name (not reached) |
| VU | 🟢 Operational | VU0 macro mode (float registers, ACC, Q, masked arithmetic, VU0 data memory instructions, clip-test status flags); VU1 microprogram interpreter for the instructions the game's programs use, with Q and clip-flag latency and XGKICK to the GS ([plan](plans/graphics/VU1_PIPELINE_PLAN.md)); VU1 memory not yet compared with a PCSX2 capture The heaviest VU1 programs are hand-written native code (three programs, six keys), checked against the interpreter on every start of the scripted fight |
| IPU / video | 🟢 Operational | MPEG-2 decoding through the game's Sofdec library: FDEC, VDEC, BDEC, CSC (RGB32), toIPU/fromIPU DMA; both movies play; IDEC, PACK and RGB16 refused; spot frames match ffmpeg and PCSX2 memory captures, no systematic comparison yet |
| Audio | 🟢 Operational | ADX stream flow native at 48 and 24 kHz, which paces movies and music, and its PCM mixed to 48 kHz stereo at the level measured against a PCSX2 recording. Sound effects: a native sampler behind SOUNDS and the sound driver (sound memory, bank parsing, SPU2 ADPCM, one voice per requested sound, looping samples, the module's keep and stop flags, slot volumes and silence requests as read from the module's code). Output to a WAV file and to the default audio device (waveOut), whose clock paces the run ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)); no volume settings Volumes for the whole mix, the streams and the effects (percent; full volume is the measured level). |
| Input | 🟢 Operational | Native controller service with scripted input, which drives the menus and a fighter in battle, or live input: keyboard, XInput pad and DualSense (HID). Keys and gamepad buttons are assigned on the Controls screen of the launcher and the in-game menu and stored in the settings file. Keyboard input was seen to reach the game; no gamepad has been tried on hardware yet ([launcher plan](plans/launcher/LAUNCHER_PLAN.md)) |
| Memory card | 🔧 Incomplete | Native card service on a host folder per card ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)): status, open, read, write, seek, flush, close, listing, change directory, make folder, set entry data, delete. Loading a save is confirmed by the user; saving and listing are not exercised by a run; replies not compared with a capture |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-13)

- **Tests:** MSVC Debug and Release builds (Ninja), **21/21 CTests** (new: the memory card folder; the menu logic and settings file; host input mapping and DualSense reports; GS readback in the sink, VIF1 and the EE sequence; stream audio flow and mixer; ADPCM decoder, sound bank parser and sampler), Python suites (174 capture, 3 CPU graphics, 91 recompilation, 11 IOP inspection, 3 Ghidra, 2 gate, 2 build). Release and Debug reach the same checkpoint. The gate is `tools/gate/run_gate.py`.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually. The logo movie's first picture (a green leader frame in a black letterbox) equals ffmpeg's decode of `LOGO_O.SFD` within one colour step; a mid-logo frame shows the same logo as a PCSX2 memory capture; the opening's first picture matches ffmpeg's. The user inspected and confirmed (2026-10-13) all sampled frames: the demo fights, the title dialogues, the main and Dueling menus, character and map selection, the loading screen and the battle. None of the 3D or menu frames has been compared with the original by capture; the pixels a GS readback returns are not compared either.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step); the order was set by the user on 2026-10-13:

1. **Sound output** ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)): streams, effects and the audio device are done; the user's listening to the looping sounds and to live play is open. Checked by tests and the user's listening; the comparison with the original is an optional final step.
2. **Controller support:** done for the keyboard, XInput pads and the DualSense with a default mapping (`run_startup_probe.py --play`); the user tries the gamepads.
3. **Launcher and settings** ([plan](plans/launcher/LAUNCHER_PLAN.md)): the player's programs exist. `bt2-game.exe` starts the game with the stored settings (the launcher first when there are none), `bt2-launcher.exe` always shows the launcher: start, controls, display (integer scaling, fullscreen, window size, frame rate display), sound volumes, game disc. In the game the in-game menu (Escape, the PS button, both stick buttons) pauses and offers the same settings, fast forward and quitting. The user tries them. Open: saves (a memory card image behind the native card service, and bringing in the user's save file), then HD textures.
4. **Play-testing by the user:** live input with automatic recording of the input as a pad script; a session that stops at a boundary is replayed through `discovery_loop.py`. Covers the other menu entries and story mode. Needs battles at real-time speed first.
5. **Performance** ([plan](plans/ee/PERFORMANCE_PLAN.md)), after the saves. Requirements from the user: fights a flat 30 pictures per second (confirmed on the original in PCSX2), menus a flat 60, also on a mid- to low-end PC. Menus are flat at 60. The fight averaged 30 with VU1's programs translated to native code, with single pictures late and no margin in the heaviest stretch; it has changed since (the opponent now acts) and is to be measured again. Decided by the user: the VU1 programs become hand-written native code, program by program, checked with the verification mode. Then: the late pictures, graphics on its own thread, presenting without readback.
6. **3D verification** (deferred to a later pass by the user on 2026-10-13): compare VU1 memory, a 3D frame and a GS readback buffer with PCSX2.
7. **Movie verification:** a frame-by-frame comparison of decoded movie frames with ffmpeg ([IPU plan](plans/video/IPU_PLAN.md)).
8. **CPU renderer in 3D** (optional, at the end): perspective-correct texturing, fog and the other states the 3D packets use, for systems without a suitable Vulkan GPU and for a mirror comparison of 3D frames.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
