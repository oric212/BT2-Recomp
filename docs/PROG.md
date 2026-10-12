# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). Baseline: verified checkpoint of 2026-10-12.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading (answered natively) |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 1,000 frames in the gate run; 7,902 in the long Release run |
| Controller check | ✅ Verified — passed with the native controller service |
| Memory card check | ✅ Verified — no card inserted; the prompt is answered by scripted input |
| Opening movie | 🟢 Operational — the logo movie and the opening movie play to the end (decoded natively, no sound); frames match an independent decoder at spot checks |
| Title screen | 🟢 Operational — the title screen and its options render (the user confirms the picture); reached with scripted Start presses |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⛔ Blocking — after New Game the game starts its 3D pipeline: VIF1 display lists with tag transfer and VU1 microprograms, not modelled yet |
| Audio | 🔧 Incomplete — sound requests and stream flow answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-12:** runtime coverage **2,007 EE functions entered / 1,998 completed** in the verified run · static coverage **10,650 EE functions mapped and generated** · verification **17/17 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-12)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 10,650, all compiled into the probe (plus 19 reached entries of the `DBZP.BIN` overlay, 473 mapped there) |
| EE functions entered / completed | 2,007 / 1,998 in the gate run (1,000 frames); 3,198 / 3,189 in the long Release run with the movies skipped |
| Presented frames / logical VBlanks | Gate run: 1,000 (frame budget) / 1,398. Long Release run (movies skipped by scripted Start): 3,385 / 4,155; with both movies played: 7,902 frames to the same sound request, then on |
| Native service requests (long run, movies skipped) | 695 disc requests, 46 CRI calls and 4,978 ADX stream commands, 36 SOUNDS calls, 78 memory card queries (no IOP code runs) |
| Native IPU (long run) | 16.5 million commands: 10.1 million header codes (VDEC), 2.46 million macroblocks (BDEC), 3,387 pictures converted to RGB (logo 151, opening 3,236) |
| Release run time | not remeasured this pass |

The gate run ends at its frame budget (1,000 frames, just after the logo movie). The long Release run (`run_startup_probe.py --config Release --frames 9000` with a pad script that presses Start) reaches the title screen, chooses New Game and stops at the first VIF1 display list of the 3D pipeline: CHCR `0x145` (chain mode with tag transfer) at pc `0x28B3E8` ([3D pipeline plan](plans/graphics/VU1_PIPELINE_PLAN.md)). The long run is Operational, not part of the gate.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code (every mapped function compiled); threads, semaphores, interrupts and priorities as reached; startup states match captures; a busy main thread still receives VBLANKs (logical CPU time); VU0 macro-mode code runs inline |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | CPU GS model (sprites, triangles, palettized textures, both circuits); paraLLEl-GS GPU backend (mirror, presentation, GPU-only; opt-in) |
| GIF / VIF | 🔧 Incomplete | GIF normal and chain-mode DMA (image data may continue across transfers), DMA CALL/RET tags, VIF1 DIRECT; VIF1 with tag transfer, UNPACK and microprogram calls not modelled |
| VU | ⛔ Blocking | VU0 macro mode (float registers, ACC, Q, masked arithmetic, VU0 data memory instructions, clip-test status flags); VU1 microprograms not modelled ([plan](plans/graphics/VU1_PIPELINE_PLAN.md)) |
| IPU / video | 🟢 Operational | MPEG-2 decoding through the game's Sofdec library: FDEC, VDEC, BDEC, CSC (RGB32), toIPU/fromIPU DMA; both movies play; IDEC, PACK and RGB16 refused; spot frames match ffmpeg and PCSX2 memory captures, no systematic comparison yet |
| Audio | 🔧 Incomplete | ADX stream flow native at 48 and 24 kHz (data announcements, played-block reports, start/stop, player destroy), which paces movies and music; SOUNDS bank load/unload and requests answered as logged; no sound output |
| Input | 🔧 Incomplete | Native controller service with scripted input; no host input yet |
| Memory card | 🔧 Incomplete | No card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-12)

- **Tests:** MSVC Debug and Release builds, **17/17 CTests**, Python suites (174 capture, 3 CPU graphics, 71 recompilation, 11 IOP inspection, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually. The logo movie's first picture (a green leader frame in a black letterbox) equals ffmpeg's decode of `LOGO_O.SFD` within one colour step; a mid-logo frame shows the same logo as a PCSX2 memory capture; the opening's first picture matches ffmpeg's. The user confirms how the game should look on request.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step):

1. **3D pipeline** (current blocker): VIF1 display lists with tag transfer, UNPACK and microprogram upload, then VU1 execution and PATH1 output ([plan](plans/graphics/VU1_PIPELINE_PLAN.md)).
2. **GPU rendering:** paraLLEl-GS draws and presents the frames; one-step rounding differences from frame 30 need a PCSX2 GS dump to decide before the GPU becomes the default.
3. **Movie verification:** a frame-by-frame comparison of decoded movie frames with ffmpeg ([IPU plan](plans/video/IPU_PLAN.md)).
4. **Build time:** link step of the full probe (about 30 s per incremental build); the generated code could move into its own library.
5. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
