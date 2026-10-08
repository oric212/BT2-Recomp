# Current progress

Target: *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441), native Windows x64 static recompilation. Updated: 2026-10-08.

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg)

**Where the port is:** the recompiled game boots, completes its startup and runs its own frame loop. It has presented **650 frames** in a live window: the controller check and the memory card screens, up to the transition into the opening movie. It then prepares the movie and stops at the next unimplemented hardware feature. Playing the movie needs MPEG decoding, which is not implemented yet.

| Milestone | Status |
| --- | --- |
| Recompilation pipeline (validation, map, code generation, native host) | ✅ Verified |
| Startup: kernel services, original IOP modules, disc reads, threads | ✅ Verified — reaches the main frame loop; key states match original captures |
| First presented frame | ✅ Verified — boot screens drawn by the GS model and shown in a live window |
| Controller and memory card checks | ✅ Verified — emulated DualShock 2; no card mounted, prompt answered by scripted input |
| Opening movie | 🔧 In progress — decoder setup done; graphics DMA chain mode next, then MPEG decoding |
| Title screen and attract-mode demo fight | ⏳ Pending |
| Menus and playable versus battle | ⏳ Pending |
| Audio output | ⏳ Pending — sound driver set up; no sound produced |
| Memory card saves | ⏳ Pending |

**Last measured, 2026-10-08:** **1,254 game functions entered / 1,243 completed**, **650 presented frames**, **1,034 logical VBlanks**, **186 original IOP disc reads**, **43 CRI sound calls**, **77 memory card queries**.
<!-- progress-summary:end -->

## What runs today

**Startup.** The game's own code, from its entry point and C library on, runs recompiled; kernel services are provided by the runtime. The original IOP modules (disc, file I/O, sound, controller, memory card, CRI middleware) are recompiled too and run natively, with SIF RPC between the two processors. Main and its worker threads are scheduled, VBlank and timer interrupts run in logical time, and the first archive table and the `DBZP.BIN` overlay load from the disc byte-identical to the original.

**Frames.** The overlay's frame loop sends its graphics through GIF and VIF1 DMA into an owned GS model. Supported so far: sprites and triangles, PSMT4/PSMT8 textures with palettes, and both display circuits. Frames are shown live with `run_startup_probe.py --viewer` (see [BUILDING.md](BUILDING.md)). An emulated DualShock 2 gets the game past its controller check. The memory card check runs through the original card modules with no card inserted, and scripted input answers its "Continue anyway?" prompt. The game then resets the GS for its next scene, and that reset matches the original.

**Toward the opening movie (current work).** The game loads the movie decoder's (IPU) tables and copies its scratchpad memory out to main RAM. It then starts its frame-pacing threads: worker 7 serves the per-frame callbacks, main sleeps at frame end, and worker 4 wakes it. The run stops at a GIF DMA transfer in chain mode (`0x1000A030`, pc `0x312084`), which no earlier screen used. Nothing new is drawn after frame 650 yet.

## Next steps

1. **GIF DMA chain mode:** the current blocker ([frame GIF plan](FRAME_GIF_PLAN.md)). It probably carries the movie scene's first graphics.
2. **Movie decoding:** real IPU MPEG-2 decoding, a new subsystem that needs its own plan.
3. **Title screen and demo fight:** after the movie, the title screen waits for Start. Without input, the game runs a CPU-vs-CPU demo fight, a good target that needs no input.
4. **Later:** VU microprograms, audio output, a mounted memory card with save import, host controller input.

Resume details: [active-pass handoff](progress/ACTIVE_PASS.md).

## Measurements

| Area | Last measured (2026-10-08) |
| --- | --- |
| EE (game CPU) | 1,254 distinct functions entered / 1,243 completed; 7,660,652 calls / 7,660,189 returns |
| Mapping | About 9,500 provisional functions mapped and generated; the overlay is mapped separately |
| IOP (I/O CPU) | 10,880,397 instructions; 15 module loads; controller polled every field |
| Threads and time | 1,034 logical VBlanks; 151 Timer 2 interrupts; 2,068 worker wakes; main sleep/wake running |
| Disc | 186 original disc reads, including streamed multi-chunk reads; archive table and overlay byte-identical |
| Sound | 43 CRI calls; sound blocks uploaded and matched to the original; SPU2 voices set up with no sample playback |
| Memory card | 77 queries, absent card only; the frame-phase reply matches the original |
| GS | 650 frames; 23,854 VIF1 DIRECT transfers; 330,667 sprites and 2,618 triangles; no VU execution yet |

Counts measure observed execution, not completed gameplay or a completion percentage. The numeric snapshot is in [latest.json](progress/latest.json); dashboard scopes are in [components.json](progress/components.json).

## Verification

Verified 2026-10-08:
- **Build and tests:** MSVC Debug and Release builds, **18/18 CTests** and the Python suites (180 capture, 69 recompilation, 13 IOP, 3 Ghidra). Release and Debug reach the same checkpoint.
- **Original comparisons:** **23 normal-run comparisons** and all **nine phase comparisons** (`BT2_PHASE_STOP`) against PCSX2 captures of the original game. Startup-scoped worker checks run at the phase stops, because later frame-phase scheduling changes that state. The remaining failures are known phase-scoped or argument-only cases.
- **Frames:** inspected visually.

Proprietary inputs, generated code, captures and rendered frames stay local and ignored.

## History and working rules

- [October 2026 history](progress/2026-10.md): every pass's evidence, earlier checkpoints and the previous detailed status.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [runtime layout](RUNTIME_LAYOUT.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md), [tracking rules](PROGRESS_TRACKING.md).

A plain "continue" runs the permanent subsystem-scale pass rule (AGENTS.md). Keep this page short; detailed pass results belong in the monthly history.
