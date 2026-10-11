# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). Baseline: verified checkpoint of 2026-10-11.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading (answered natively) |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 650 frames shown in a live window |
| Controller check | ✅ Verified — passed with the native controller service |
| Memory card check | ✅ Verified — no card inserted; the prompt is answered by scripted input |
| Opening movie | ⛔ Blocking — the native IPU reads the movie stream and its header; frame decoding is next |
| Title screen | ⏳ Not Reached — waits for Start |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⏳ Not Reached |
| Audio | 🔧 Incomplete — sound requests answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-12:** runtime coverage **1,602 EE functions entered / 1,586 completed** in the verified run · static coverage **9,504 EE functions mapped and generated** · verification **17/17 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-12)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 9,504 (456 in the `DBZP.BIN` overlay; 1,780 + 12 compiled into the probe) |
| EE functions entered / completed | 1,602 / 1,586 (kernel stubs 2 / 2) |
| Presented frames / logical VBlanks | 650 / 1,037 |
| Native service requests | 186 disc reads, 43 CRI calls and 2 ADX stream commands, 77 memory card queries (no IOP code runs) |
| Release run time | about 19 s, dominated by the CPU GS rasterizer |

The run ends in the movie decoder's wait loop after the MPEG sequence header (execution budget, thread 7, pc `0x31A3C8`) ([IPU plan](plans/video/IPU_PLAN.md)).

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code; threads, semaphores, interrupts and priorities as reached; startup states match captures |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | CPU GS model (sprites, triangles, palettized textures, both circuits); paraLLEl-GS GPU backend (mirror, presentation, GPU-only; opt-in) |
| GIF / VIF | 🟢 Operational | GIF normal and chain-mode DMA, VIF1 DIRECT; VIF unpack not reached |
| VU | ⏳ Not Reached | |
| IPU / video | ⛔ Blocking | toIPU chain input, bit reader and FDEC; the decoder waits after the sequence header |
| Audio | 🔧 Incomplete | Requests and the reached ADX stream commands answered; no output |
| Input | 🔧 Incomplete | Native controller service with scripted input; no host input yet |
| Memory card | 🔧 Incomplete | No card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-11)

- **Tests:** MSVC Debug and Release builds, **17/17 CTests**, Python suites (174 capture, 3 CPU graphics, 70 recompilation, 11 IOP inspection, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step):

1. **GPU rendering:** paraLLEl-GS draws and presents the startup frames (GPU-only run 2.3 s vs 19.1 s); one-step rounding differences from frame 30 need a PCSX2 GS dump to decide before the GPU becomes the default.
2. **Opening movie** (current blocker): native IPU decoding for the game's Sofdec library ([IPU plan](plans/video/IPU_PLAN.md)).
3. **GIF channel models:** merge the startup GIF channel-2 models into the frame one.
4. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
