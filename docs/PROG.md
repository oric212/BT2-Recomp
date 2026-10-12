# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). Baseline: verified checkpoint of 2026-10-12.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading (answered natively) |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 1,000 frames presented (the probe's frame budget) |
| Controller check | ✅ Verified — passed with the native controller service |
| Memory card check | ✅ Verified — no card inserted; the prompt is answered by scripted input |
| Opening movie | ⛔ Blocking — the native IPU decodes and presents the first pictures; playback then waits for audio progress |
| Title screen | ⏳ Not Reached — waits for Start |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⏳ Not Reached |
| Audio | 🔧 Incomplete — sound requests answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-12:** runtime coverage **1,829 EE functions entered / 1,819 completed** in the verified run · static coverage **9,517 EE functions mapped and generated** · verification **17/17 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-12)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 9,517 (456 in the `DBZP.BIN` overlay; 1,964 + 12 compiled into the probe) |
| EE functions entered / completed | 1,829 / 1,819 (kernel stubs 2 / 2) |
| Presented frames / logical VBlanks | 1,000 (frame budget; 350 of them movie fields) / 1,383 |
| Native service requests | 572 disc requests, 43 CRI calls and 21 ADX stream commands, 78 memory card queries (no IOP code runs) |
| Native IPU | 4,821 commands: 3,056 header codes (VDEC), 1,088 macroblocks (BDEC), 2,688 macroblocks converted to RGB (three pictures) |
| Release run time | not remeasured this pass |

The run ends at the frame budget (1,000 presented frames) inside the opening movie: three pictures are decoded and presented, then the player waits for audio playback progress, which the native CRI service does not report yet ([IPU plan](plans/video/IPU_PLAN.md), [Sofdec plan](plans/video/SOFDEC_PLAN.md)). Without the budget the run stops after 3,600 fields without a new frame.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code; threads, semaphores, interrupts and priorities as reached; startup states match captures |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | CPU GS model (sprites, triangles, palettized textures, both circuits); paraLLEl-GS GPU backend (mirror, presentation, GPU-only; opt-in) |
| GIF / VIF | 🟢 Operational | GIF normal and chain-mode DMA (image data may continue across transfers), VIF1 DIRECT; VIF unpack not reached |
| VU | ⏳ Not Reached | |
| IPU / video | 🔧 Incomplete | MPEG-2 decoding through the game's Sofdec library: FDEC, VDEC, BDEC, CSC (RGB32), toIPU/fromIPU DMA; IDEC, PACK and RGB16 refused; frames not yet compared with PCSX2 |
| Audio | ⛔ Blocking | Requests and the reached ADX stream commands answered; no output and no playback progress, which the movie player waits for |
| Input | 🔧 Incomplete | Native controller service with scripted input; no host input yet |
| Memory card | 🔧 Incomplete | No card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-12)

- **Tests:** MSVC Debug and Release builds, **17/17 CTests**, Python suites (174 capture, 3 CPU graphics, 71 recompilation, 11 IOP inspection, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually; the first movie picture (a flat green field inside a black letterbox) is identical in the CPU and GPU renderers but not yet compared with PCSX2.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step):

1. **GPU rendering:** paraLLEl-GS draws and presents the startup frames (GPU-only run 2.3 s vs 19.1 s); one-step rounding differences from frame 30 need a PCSX2 GS dump to decide before the GPU becomes the default.
2. **Opening movie** (current blocker): find how the movie player learns audio playback progress and supply it natively ([Sofdec plan](plans/video/SOFDEC_PLAN.md#progress-2026-10-12-later)); then compare decoded frames with PCSX2 ([IPU plan](plans/video/IPU_PLAN.md)).
3. **GIF channel models:** merge the startup GIF channel-2 models into the frame one.
4. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
