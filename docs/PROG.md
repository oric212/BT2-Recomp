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
| Title screen | ⛔ Blocking — after the movies the game asks its sound module for something not yet captured (SOUNDS `0x50`) |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⏳ Not Reached |
| Audio | 🔧 Incomplete — sound requests answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-12:** runtime coverage **2,007 EE functions entered / 1,998 completed** in the verified run · static coverage **9,547 EE functions mapped and generated** · verification **17/17 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-12)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 9,547 (456 in the `DBZP.BIN` overlay; 2,124 + 12 compiled into the probe) |
| EE functions entered / completed | 2,007 / 1,998 in the gate run (1,000 frames); 2,016 / 2,005 in the long Release run |
| Presented frames / logical VBlanks | Gate run: 1,000 (frame budget) / 1,398. Long Release run (9,000-frame budget): 7,902 / 8,716 |
| Native service requests (long run) | 2,546 disc requests, 46 CRI calls and 13,568 ADX stream commands, 78 memory card queries (no IOP code runs) |
| Native IPU (long run) | 16.5 million commands: 10.1 million header codes (VDEC), 2.46 million macroblocks (BDEC), 3,387 pictures converted to RGB (logo 151, opening 3,236) |
| Release run time | not remeasured this pass |

The gate run ends at its frame budget (1,000 frames, just after the logo movie). The long Release run plays both movies to the end and stops at the native SOUNDS service's checked boundary: function `0x50` (256-byte command, words `0, 0, 0, 0x1F`), whose reply must be captured ([Sofdec plan](plans/video/SOFDEC_PLAN.md), [IPU plan](plans/video/IPU_PLAN.md)). The long run is Operational, not part of the gate.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code; threads, semaphores, interrupts and priorities as reached; startup states match captures; a busy main thread still receives VBLANKs (logical CPU time) |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | CPU GS model (sprites, triangles, palettized textures, both circuits); paraLLEl-GS GPU backend (mirror, presentation, GPU-only; opt-in) |
| GIF / VIF | 🟢 Operational | GIF normal and chain-mode DMA (image data may continue across transfers), VIF1 DIRECT; VIF unpack not reached |
| VU | ⏳ Not Reached | |
| IPU / video | 🟢 Operational | MPEG-2 decoding through the game's Sofdec library: FDEC, VDEC, BDEC, CSC (RGB32), toIPU/fromIPU DMA; both movies play; IDEC, PACK and RGB16 refused; spot frames match ffmpeg and PCSX2 memory captures, no systematic comparison yet |
| Audio | ⛔ Blocking | ADX stream flow native (data announcements, played-block reports at 48 kHz, start/stop), which paces the movies; no sound output; SOUNDS `0x50` not captured |
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

1. **GPU rendering:** paraLLEl-GS draws and presents the startup frames (GPU-only run 2.3 s vs 19.1 s); one-step rounding differences from frame 30 need a PCSX2 GS dump to decide before the GPU becomes the default.
2. **Title screen** (current blocker): capture the reply to SOUNDS function `0x50` in PCSX2 (a new capture point after the opening movie) and answer it natively; then continue to the title screen.
3. **Movie verification:** a frame-by-frame comparison of decoded movie frames with ffmpeg ([IPU plan](plans/video/IPU_PLAN.md)).
4. **GIF channel models:** merge the startup GIF channel-2 models into the frame one.
5. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
