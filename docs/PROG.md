# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-08.

## Verified checkpoint

<!-- progress-summary:start -->
![BT2 component status map](progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Native host, retail validation and recompilation pipeline | ✅ Verified |
| Generated BT2 startup execution | 🔧 Partial — reaches a checked runtime boundary |
| Initial disc read and metadata lookups | ✅ Verified |
| First archive (AFS) table load | ✅ Verified — byte-identical to original |
| Complete startup and thread scheduling | 🔧 In progress — startup reaches main's frame loop; VBlank, display setup, Timer 2, streamed reads, sound bank verified |
| Game asset loading and first presented frame | 🔧 In progress — boot screens rendered and shown in a live window; controller and memory card checks pass; scripted input answers the no-card prompt (649 frames) |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Verified blocker (2026-10-08):** after the memory card prompt (answered Yes by scripted input), the game resets the GS for its next scene. That frame-phase reset now runs and matches a new original capture. The game then stops at the next unselected function (`FUN_002B7938`).

**Last measured, 2026-10-08:** **804 EE entries entered / 793 completed**, **650 rendered frames**, **42 CRI calls**, **186 original IOP disc reads**, **1,033 logical VBlanks**, **77 memory card queries**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.

<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 804 distinct entered / 793 completed; kernel 2 / 2; 7,657,836 call events / 7,657,373 returns over 649 frames |
| Mapping | 9,446 provisional EE entries; 608 extra entries selected; overlay map 456 entries, 8 selected |
| IOP | 10,866,546 instructions; scripted DualShock 2 input (`--pad-script`); IOP VBlank start/end every field (pad, SIO2D and CRI threads run); emulated DualShock 2 on port 1 (configured into pressure mode, polled each field); 5 DBCMAN and 50 controller-port requests through the original handlers; CRI stream buffers returned to the EE; SPU2 voice positions/ENDX (no samples/ADSR); 132 logical SOUNDS timer ticks; 284 physical entry targets; 15 load paths (embedded module reloaded) / 13 resident modules |
| Memory / threads | Nine IOP heap allocations / two frees (two sound-bank blocks at captured placements); four EE startup pools; main priority 2 → 24 admitted; 117 logical VBLANK_START deliveries (+2 polled); 234 owned helper wakes/resumes; logical Timer 2 with 16 overflow interrupts |
| Disc | Directory read plus 56 archive reads (57 total) through original CDVDFSV handlers, including main's first synchronous file load and a 52-sector streamed read (4 ring chunks, bytes equal the ISO); first AFS table (0x6880 bytes) byte-identical to original |
| Sound | 42 CRI calls (all five second-channel pairs); three EE→IOP sound uploads; two sound-driver commands through the original handlers with owned SPU2 voice transfers (DMA7 + IRQ 40); IOP sound block and bank blocks match the original; SOUNDS worker started with its IOP hardware timer; first 2,048-byte AutoDMA half programmed; two EE→IOP CRI stream-buffer copies; no consumption / completion IRQ / native playback verified |
| Cards | 77 queries (startup plus one per frame while the card screens run), each six DMA pairs / five timed FIFO probes; absent cards only; frame-phase reply matches the original |
| GS | Startup: five packets / four clear draws; frames: 649 flips, 23,854 VIF1 DIRECT transfers, PSMT4 and PSMT8 textures with 16- and 256-entry CLUTs, 330,666 sprites and 2,618 triangles (530,581,870 pixels); memory card dialogs rendered; no VU execution |

Previous normal checkpoint → current: distinct EE **772 / 763 → 801 / 791**. The no-card prompt renders (PSMT8 textures), Timer 1 wraps past long frames, and scripted input answers the prompt, so the game runs 649 frames to its next scene setup. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified 2026-10-08:
- **Build and unit tests:** serialized MSVC entry/ALL builds and **17/17 CTests**, with new PSMT8 layout/CLUT, Timer 1 wrap and pad-script tests.
- **Python suites:** **180 capture**, **68 recompilation**, **13 IOP** and 3 Ghidra tests.
- **Comparisons:** **27 normal-startup comparisons** and all nine phase-matched comparisons (`BT2_PHASE_STOP`), including the frame-phase GS reset against an interactive original capture. The remaining failures are the known phase-scoped and argument cases.
- **Diagnostics:** the sound-worker diagnostic is unchanged at 302/295; rendered frames were inspected visually.
- **No behavior change from the performance work:** the GS rasterizer optimizations and the Release build produce reports and RAM/GS dumps byte-identical to Debug and to the previous code.

Proprietary inputs, generated output and rendered frames remain ignored.

## Current blocker and next step

Enabling VBLANK_START delivers the latched interrupt at once; original captures place it after the enable and before the request. Handlers run with the kernel ABI on the captured interrupt stack; iWakeupThread readies sleeping helpers 5/6, which preempt main and re-park. Main's request poll then uses one logical VBlank per idle call. Helper 6 drives `zs2us_1.afs` through original CDVDFSV readiness, read and error-query handlers. Main leaves the poll with the loaded 0x6880-byte table identical to the original capture (original: 120 VBlanks; timing not equated). The first R5900 FPU operations now use authored round-toward-zero, non-IEEE semantics.

All five late CRI `0x408` pairs complete through the original IOP handler; their objects match the original. Main then repeats its startup clear routine: GS reset, VSync handler removal, a polled VSync, both GIF clears against the original packets. It samples the field, reloads the embedded IOP module at the original heap placement and sets up the 512×448 display with a re-registered VSync callback. Double-buffer environment, field sample and GS library state match the original. EE Timer 2 now counts in logical time (one NTSC field per VBlank); each wrap runs the original overflow handler, and the timer library state matches the original. Main seeds its random state and loads its first file synchronously through original disc reads. Reads above 16 sectors now stream through the original FSV handler's 16-sector ring: each chunk restarts CDVD DMA channel 3 (retail block layout) and raises its own callback. Helper 6's 52-sector read completes in four chunks and its EE bytes equal the ISO. Main's post-load setup then reads two more files, uploads a 0x16640-byte sound block into the IOP sound buffer (one SIF DMA; bytes verified) and sends sound-driver command `0x80D0`. Its LIBSD voice transfers into SPU2 sound RAM now run through an owned DMA7 model with the original IRQ 40 handler ([SPU2 plan](IOP_SPU2_TRANSFER_PLAN.md)); two sound-driver commands complete. The loader then places two bank blocks at the original IOP heap placements and uploads them (bytes equal the original). Its final SOUNDS command `0x20` starts the SOUNDS worker and its 240 Hz IOP hardware timer, which now ticks in logical time ([timer plan](IOP_HARDWARE_TIMER_PLAN.md)). Main then enters its frame loop. The first frame step opens `cdrom0:\BIN\DBZP.BIN` through the C library's file API; stdio initialization binds boot FILEIO's file server and its first request runs through the original handler. The open, two seeks, the read and the close then run through FILEIO's per-file worker thread, and each completes through an IOP→EE SIF command into the C library's handler ([EE file I/O plan](EE_FILEIO_PLAN.md)). The loaded `DBZP.BIN` equals the original (`rpc-dbzp-loaded`). Main then calls into it. The overlay is analysed and generated as a separate program; its functions run only while RAM holds the bytes they were generated from ([EE overlay plan](EE_OVERLAY_PLAN.md)). Its frame path starts Timer 1 for an H-BLANK delay and flips the display environment. The display block, the overlay range and the recorded GS display registers (512×448 interlaced) match the original (`rpc-overlay-dispenv`). Its first frame then runs through general frame GIF DMA ([frame GIF plan](FRAME_GIF_PLAN.md)):
- **Draw environment:** the send clears the whole 512×448 framebuffer in the owned VRAM model (229,376 pixels).
- **Path sync and render kick:** the GS library's path sync sees idle paths, and the game's render kick sends an empty VIF1 chain.
- **Display setup:** the game's own display writes configure both read circuits.

The frame's Timer 1 wait then crosses field boundaries: the IOP SOUNDS timer ticks in logical time, and its worker reads SPU2 ENDX from an owned voice position model ([SPU2 voice plan](SPU2_VOICE_PLAN.md)). The frame's one-shot VSync flag is published as in the original. The overlay then loads its first indexed asset through CRI's file layer: each status poll is a logical idle point, and the read completes through the original disc handlers. Two frame-phase controller-port requests run through the original DBC handlers. From the second frame on, each render kick sends VIF1 DIRECT GS work: a PSMT4 texture and CLUT upload and a textured sprite ([GS texture plan](GS_TEXTURE_PLAN.md)). Without a controller, the rendered framebuffer shows the game's "No DUALSHOCK®2 analog controller connected" message; frames can be watched live (`run_startup_probe.py --viewer`, [live window plan](LIVE_WINDOW_PLAN.md)). The native IOP now receives VBlank start/end every field, so the original pad, SIO2 and CRI threads run. An emulated DualShock 2 answers the pad protocol on port 1 ([controller plan](CONTROLLER_PLAN.md)). With it connected, the game leaves that screen and runs its memory card check through the original MCSERV/MCMAN/SIO2MAN modules with absent cards. Its "Continue anyway?" prompt is answered by scripted input. The game then resets the GS for its next scene: the reset runs and matches the original. The next boundary is the unselected function `FUN_002B7938`. The IOP stream consumer acknowledgement is still unmodelled. See the [resume handoff](progress/ACTIVE_PASS.md).

Remaining: IOP stream consumption, sound command processing, general timers, mounted cards, archive decompression/use, audio and presentation. Complete startup and a presented game frame remain unverified.

Run the measured checkpoint using the [startup probe instructions](BUILDING.md): `python tools/ps2recomp/run_startup_probe.py --disc <local-ISO-path>`.

## Milestones

- [x] Native host executable.
- [x] Local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools, provisional EE map and local generation.
- [x] Generated routines compiled/linked/executed; game entry reached.
- [x] Local kernel capture and bounded generated platform initialization.
- [x] Initial absent-card query through native IOP and generated EE callback.
- [x] Owned disc metadata lookup through original native IOP and generated EE cleanup.
- [x] First directory-sector read, generated parsing and all five listed file lookups.
- [ ] Complete startup and general required runtime integration.
- [ ] First rendered/presented frame; title screen and main menu.
- [ ] Character select and battle load.
- [ ] Playable versus battle.
- [ ] Audio and full match.

## Active pass and tracking

See [active-pass handoff](progress/ACTIVE_PASS.md) for the completed-pass outcome and next subsystem resume instructions. See [tracking rules](PROGRESS_TRACKING.md) for status definitions and README synchronization. New changes enter the verified checkpoint only after their required checks pass.

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
