# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), native Windows x64 static recompilation. Baseline: verified checkpoint of 2026-10-08.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

| Functional milestone | Status |
| --- | --- |
| Boot | ✅ Verified — game entry, kernel services, IOP reset and module loading |
| Main frame loop | ✅ Verified — key startup states match original captures |
| First presented frame | ✅ Verified — 650 frames shown in a live window |
| Controller check | ✅ Verified — passed with an emulated DualShock 2 |
| Memory card check | ✅ Verified — no card inserted; the prompt is answered by scripted input |
| Opening movie | ⛔ Blocking — decoder set up; GIF DMA chain mode stops the run before the first movie frame |
| Title screen | ⏳ Not Reached — waits for Start |
| Demo fight (no input) | ⏳ Not Reached |
| Gameplay (menus, versus battle) | ⏳ Not Reached |
| Audio | 🔧 Incomplete — sound driver set up; no sound output |
| Saves | ⏳ Not Reached — no memory card image yet |

**Measured 2026-10-08:** runtime coverage **1,254 EE functions entered / 1,243 completed** in the verified run · static coverage **9,493 EE functions mapped and generated** · verification **18/18 tests, 42 of 46 original comparisons passing** at this checkpoint.
<!-- progress-summary:end -->

## How progress is measured

| Category | What it counts |
| --- | --- |
| **Functional milestones** | How far the game gets through its own sequence, from boot to saves |
| **Static coverage** | Code mapped, generated and compiled ahead of time |
| **Runtime coverage** | Unique functions entered and completed in the verified run |
| **Subsystem status** | Which PS2 parts are implemented, and their scope |
| **Verification** | Tests, and comparisons of recompiled state with the original game in PCSX2 |

| Status | Meaning |
| --- | --- |
| ✅ **Verified** | Implemented and verified against expected/original behavior for the relevant scope. |
| 🟢 **Operational** | Implemented and demonstrably working on the paths currently exercised, but broader behavior is not yet validated. |
| 🔧 **Incomplete** | Some required functionality is still missing, but the implemented portion works. |
| ⛔ **Blocking** | Missing/incorrect functionality currently prevents the game from progressing. |
| ⏳ **Not Reached** | The game has not yet exercised enough of this subsystem to assess it. |

## Static coverage

| Item | Count |
| --- | --- |
| EE functions in the provisional map, generated as C++ | 9,493 (2.16 MB of code; 83 more quarantined for review) |
| Overlay (`DBZP.BIN`) functions mapped | 456 |
| EE entries compiled into the startup probe | 1,361 main executable + 12 overlay |
| IOP modules recompiled into the native IOP runtime | 15 module units (disc, file I/O, sound, controller, memory card, CRI) |

Map boundaries are provisional; functions are compiled when execution reaches them.

## Runtime coverage (verified run, 2026-10-08)

| Item | Count |
| --- | --- |
| EE functions entered / completed | 1,254 / 1,243 (kernel stubs 2 / 2) |
| EE calls / returns | 7,660,652 / 7,660,189 |
| IOP instructions executed | 10,880,397 (15 module loads) |
| Presented frames | 650 |
| Logical VBlanks / Timer 2 interrupts | 1,034 / 151 |
| Disc reads (original IOP handlers) | 186 |
| CRI sound calls / memory card queries | 43 / 77 |

The run stops at a checked boundary: a GIF DMA transfer in chain mode (`0x1000A030`, pc `0x312084`), before the first movie frame.

## Subsystem status

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel services | 🟢 Operational | Recompiled game code; threads, semaphores, interrupts, sleep/wake and priorities as the game reaches them; startup states match captures, frame-phase scheduling not yet compared |
| IOP | 🟢 Operational | Original modules run natively; SIF RPC to disc, file I/O, sound, controller, memory card and CRI services; the CRI lifecycle comparison fails |
| GS | 🔧 Incomplete | Software GS model: sprites, triangles, PSMT4/PSMT8 textures with palettes, both display circuits, reset |
| GIF / VIF | ⛔ Blocking | GIF normal-mode DMA and VIF1 DIRECT work; GIF chain mode stops the run; VIF unpack not reached |
| VU | ⏳ Not Reached | No VU microprogram has run yet |
| IPU / video | 🔧 Incomplete | Decoder tables, thresholds and reset; no MPEG decoding |
| Audio | 🔧 Incomplete | Sound driver, sound banks and SPU2 voice setup through the original modules; no sample playback or output |
| Input | 🔧 Incomplete | Emulated DualShock 2 with scripted input; no host controller or keyboard yet |
| Memory card | 🔧 Incomplete | Card queries through the original modules with no card inserted; no card image or saves |
| Disc / filesystem | ✅ Verified | ISO-backed CDVD reads, streaming and file I/O; archive table and overlay byte-identical to the original |

## Verification

Verified 2026-10-08:
- **Tests:** MSVC Debug and Release builds, **18/18 CTests**, and the Python suites (180 capture, 69 recompilation, 13 IOP, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Original comparisons:** 46 comparison scripts check recompiled state against PCSX2 captures of the original game.
  - 23 pass on the full run.
  - 12 startup-scoped checks pass at captured phase stops (`BT2_PHASE_STOP`, four phases).
  - 7 pass with their own inputs: six capture checks (`cd_init`, `rpc_bind`, `heap_bind`, `heap_allocation`, `heap_copy`, `spu2_transfer`), whose regenerated probe inputs are byte-identical, and `disc_read` against the early-stop sound-worker run.
  - 4 fail: `cri_rpc` and `second_heap` (native lifecycle differs), `cri_preemption` (checks a former blocker) and `spu2_initialization` (no current IOP-module-probe report).
- **Frames:** inspected visually.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## What runs today

**Startup.** The game's own code, from its entry point and C library on, runs recompiled; kernel services are provided by the runtime. The original IOP modules run natively with SIF RPC between the two processors. The first archive table and the `DBZP.BIN` overlay load byte-identical to the original.

**Frames.** The overlay's frame loop sends its graphics through GIF and VIF1 DMA into the GS model, shown live with `run_startup_probe.py --viewer` (see [BUILDING.md](guides/BUILDING.md)). The emulated DualShock 2 passes the controller check. The memory card check runs with no card, and scripted input answers its "Continue anyway?" prompt. The GS reset for the next scene matches the original.

**Toward the opening movie (current).** The game sets up the movie decoder (IPU), copies its scratchpad memory to main RAM and starts its frame-pacing threads: worker 7 serves per-frame callbacks, main sleeps at frame end, worker 4 wakes it. Nothing new is drawn after frame 650 yet.

## Next steps

The work shifts from per-blocker workarounds to general, measured implementations ([implementation plan](project/IMPLEMENTATION_PLAN.md#current-next-step-change-of-strategy)):

1. **Profile** the 650-frame Release startup run per subsystem (GS, IOP, EE, GIF/VIF, scheduler) and record the baseline.
2. **Generalize** verified handlers that are gated on guest addresses, with regression tests and comparisons kept passing.
3. **GIF DMA chain mode** as a general DMA chain walker shared with VIF1, the current blocker ([frame GIF plan](plans/graphics/FRAME_GIF_PLAN.md)).
4. **Movie decoding:** plan IPU MPEG decoding in `docs/plans/video/` before implementing it.
5. **Then:** title screen and demo fight, VU microprograms, audio output, memory card image, host controller input.

Resume details: [active-pass handoff](progress/ACTIVE_PASS.md). Numeric snapshot: [latest.json](progress/latest.json); dashboard scopes: [components.json](progress/components.json).

## History and working rules

- [October 2026 history](progress/2026-10.md): every pass's evidence and earlier checkpoints.
- [Implementation plan](project/IMPLEMENTATION_PLAN.md), [reference architecture](reference/REFERENCE_ARCHITECTURE.md), [runtime layout](guides/RUNTIME_LAYOUT.md), [tools](guides/TOOLS.md), [reverse-engineering evidence](reference/REVERSE_ENGINEERING.md), [tracking rules](project/PROGRESS_TRACKING.md).

A plain "continue" runs the permanent subsystem-scale pass rule (AGENTS.md). Keep this page short; detailed pass results belong in the monthly history.
