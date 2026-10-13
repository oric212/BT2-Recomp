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
| Title screen | 🟢 Operational — the title screen and its options render (the user confirms the picture); reached with scripted Start presses |
| Demo fight (no input) | 🟢 Operational — two CPU-vs-CPU demo fights render in 3D on the GPU renderer and play to their end (about 5,600 frames; frames inspected, not compared with the original); no sound |
| Gameplay (menus, versus battle) | ⏳ Not Reached — the scripted Start presses did not leave the title/demo cycle; the menu path is next |
| Audio | 🔧 Incomplete — sound requests and stream flow answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-13:** runtime coverage **2,007 EE functions entered / 1,998 completed** in the verified run (4,854 / 4,847 in the long run) · static coverage **10,680 EE functions mapped and generated** · verification **18/18 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-13)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 10,680, all compiled into the probe (plus 19 reached entries of the `DBZP.BIN` overlay, 473 mapped there) |
| EE functions entered / completed | 2,007 / 1,998 in the gate run (1,000 frames); 4,854 / 4,847 in the long Release run with the movies skipped |
| Presented frames / logical VBlanks | Gate run: 1,000 (frame budget) / 1,398. Long Release run (movies skipped by scripted Start): 9,000 (frame budget) / 12,824 |
| Native service requests (long run, movies skipped) | 320 disc read commands, 46 CRI calls, 370 SOUNDS calls, 76 memory card queries (no IOP code runs) |
| 3D pipeline (long run) | 8,903 VIF1 display lists: 14.2 million UNPACKs, 48,963 microprogram uploads, 7.9 million VU1 program starts (4.43 billion VU1 instructions), 10.5 million XGKICK packets; 90 million triangles and 2 million sprites reach the GS. No VU1 read of Q or the clip flags fell inside a pipeline delay |
| Native IPU (long run) | 16.5 million commands: 10.1 million header codes (VDEC), 2.46 million macroblocks (BDEC), 3,387 pictures converted to RGB (logo 151, opening 3,236) |
| Release run time | 214 s for the 9,000-frame long run, of which about 5,600 frames are 3D demo fights (VU1 interpreted, GPU renderer; 10-core machine). Not a gameplay performance figure |

The gate run ends at its frame budget (1,000 frames, just after the logo movie). The long Release run (`run_startup_probe.py --config Release --frames 9000` with a pad script that presses Start) skips the movies, shows the title screen and then the game's idle cycle: a demo fight, the title screen again, a second demo fight ([frames](progress/ACTIVE_PASS.md#frames-to-inspect)); it ends at the frame budget. The earlier note that the Start presses chose New Game was wrong: the stop of 2026-10-12 was the first display list of the demo fight. The long run is Operational, not part of the gate.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code (every mapped function compiled); threads, semaphores, interrupts and priorities as reached; startup states match captures; a busy main thread still receives VBLANKs (logical CPU time); VU0 macro-mode code runs inline. Two generator errors are corrected when sources are prepared: the operand of `SQRT.S` and writes to the constant register VF0 |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | paraLLEl-GS GPU backend draws the 3D scenes (GPU-only mode: the CPU model checks packet framing only). CPU GS model (sprites, triangles, palettized textures, both circuits) draws the 2D screens; it refuses 3D drawing states (perspective texturing, fog), so the CPU fallback and the mirror comparison stop at the first 3D frame |
| GIF / VIF | 🟢 Operational | GIF normal and chain-mode DMA (image data may continue across transfers), DMA CALL/RET tags; VIF1 display lists with tag transfer: state codes, MPG, UNPACK (S and V4 forms, plain write cycle), MSCAL/MSCALF/MSCNT with the double buffer, DIRECT/DIRECTHL. V2/V3 UNPACK, masking, addition modes, MSKPATH3 and code interrupts are refused by name (not reached) |
| VU | 🟢 Operational | VU0 macro mode (float registers, ACC, Q, masked arithmetic, VU0 data memory instructions, clip-test status flags); VU1 microprogram interpreter for the instructions the game's programs use, with Q and clip-flag latency and XGKICK to the GS ([plan](plans/graphics/VU1_PIPELINE_PLAN.md)); VU1 memory not yet compared with a PCSX2 capture |
| IPU / video | 🟢 Operational | MPEG-2 decoding through the game's Sofdec library: FDEC, VDEC, BDEC, CSC (RGB32), toIPU/fromIPU DMA; both movies play; IDEC, PACK and RGB16 refused; spot frames match ffmpeg and PCSX2 memory captures, no systematic comparison yet |
| Audio | 🔧 Incomplete | ADX stream flow native at 48 and 24 kHz (data announcements, played-block reports, start/stop, player destroy), which paces movies and music; SOUNDS bank load/unload and requests answered as logged; no sound output |
| Input | 🔧 Incomplete | Native controller service with scripted input; no host input yet |
| Memory card | 🔧 Incomplete | No card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-13)

- **Tests:** MSVC Debug and Release builds (Ninja), **18/18 CTests** (new: VIF1 codes and VU1 instructions), Python suites (174 capture, 3 CPU graphics, 85 recompilation, 11 IOP inspection, 3 Ghidra, 2 gate, 2 build). Release and Debug reach the same checkpoint. The gate is `tools/gate/run_gate.py`.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually. The logo movie's first picture (a green leader frame in a black letterbox) equals ffmpeg's decode of `LOGO_O.SFD` within one colour step; a mid-logo frame shows the same logo as a PCSX2 memory capture; the opening's first picture matches ffmpeg's. Six sampled frames of the demo fights (characters, stages, effects, the title overlay) look plausible; the user is asked to confirm them. The user confirms how the game should look on request.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step):

1. **Menus and battle setup:** find the input that leaves the title screen (the scripted Start presses did not), then follow New Game through its boundaries toward character selection.
2. **3D verification:** compare VU1 memory and a 3D frame with PCSX2 (a VU1 capture point for `capture_ee.py`, a GS dump or screenshot); the user inspects the saved demo frames.
3. **CPU renderer in 3D:** perspective-correct texturing, fog and the other states the 3D packets use, so the fallback renderer and the mirror comparison cover 3D frames.
4. **Performance:** measure Release frame time in the fights; translate the hot VU1 microprograms to native code if the interpreter is the limit.
5. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.
6. **Movie verification:** a frame-by-frame comparison of decoded movie frames with ffmpeg ([IPU plan](plans/video/IPU_PLAN.md)).

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
