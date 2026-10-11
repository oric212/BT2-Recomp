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
| Opening movie | ⛔ Blocking — decoder set up; GIF DMA chain mode stops the run before the first movie frame |
| Title screen | ⏳ Not Reached — waits for Start |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⏳ Not Reached |
| Audio | 🔧 Incomplete — sound requests answered natively; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-11:** runtime coverage **1,254 EE functions entered / 1,243 completed** in the verified run · static coverage **9,493 EE functions mapped and generated** · verification **15/15 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Measurements (verified run, 2026-10-11)

| Item | Count |
| --- | --- |
| EE functions mapped and generated | 9,493 (456 in the `DBZP.BIN` overlay; 1,361 + 12 compiled into the probe) |
| EE functions entered / completed | 1,254 / 1,243 (kernel stubs 2 / 2) |
| Presented frames / logical VBlanks | 650 / 1,034 |
| Native service requests | 186 disc reads, 43 CRI calls, 77 memory card queries; 1,008 pad blocks (no IOP code runs) |
| Release run time | about 19 s, dominated by the CPU GS rasterizer |

The run stops at a checked boundary: a GIF DMA transfer in chain mode (`0x1000A030`, pc `0x312084`), before the first movie frame.

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Recompiled game code; threads, semaphores, interrupts and priorities as reached; startup states match captures |
| IOP services | 🟢 Operational | Native services for disc, files, IOP heap, LOADFILE, controller, memory card, sound driver, SOUNDS and CRI ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)); EE state identical to the former emulated IOP |
| GS | 🔧 Incomplete | CPU GS model (sprites, triangles, palettized textures, both circuits); GPU path via paraLLEl-GS planned |
| GIF / VIF | ⛔ Blocking | GIF normal-mode DMA and VIF1 DIRECT work; GIF chain mode stops the run |
| VU | ⏳ Not Reached | |
| IPU / video | 🔧 Incomplete | Decoder setup only; native Sofdec playback planned |
| Audio | 🔧 Incomplete | Requests answered; no output; ADX stream commands are a checked boundary |
| Input | 🔧 Incomplete | Native controller service with scripted input; no host input yet |
| Memory card | 🔧 Incomplete | No card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed reads and file I/O; archive table and overlay byte-identical to the original |

## Verification (2026-10-11)

- **Tests:** MSVC Debug and Release builds, **15/15 CTests**, Python suites (177 capture, 69 recompilation, 11 IOP inspection, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Native services A/B:** same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes ([details](plans/iop/NATIVE_SERVICES_PLAN.md#progress-2026-10-11)).
- **Original comparisons** (46 scripts against PCSX2 captures): 23 pass on the full run, 12 at captured phase stops, 7 with their own capture inputs (not rerun this pass); 4 fail (`cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`), as before.
- **Frames:** inspected visually.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## Next steps

Per the [implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step):

1. **GPU rendering:** integrate paraLLEl-GS and match it against the CPU renderer on the startup frames.
2. **GIF DMA chain mode** (current blocker), with the GIF channel models consolidated into one.
3. **Opening movie:** plan native Sofdec (SFD) playback, then implement it.
4. **Native audio output:** ADX decoding and sound bank playback behind the sound and CRI services.

## Links

[Active handoff](progress/ACTIVE_PASS.md) · [October 2026 history](progress/2026-10.md) · [latest.json](progress/latest.json) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md) · [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md)
