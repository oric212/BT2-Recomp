# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-07.

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
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** main calls into the loaded `DBZP.BIN` overlay (`0x3B7FA8`); overlay code is not analysed or generated yet ([EE overlay plan](EE_OVERLAY_PLAN.md)).

**Last measured, 2026-10-07:** **601 EE entries entered / 595 completed**, **42 CRI calls**, **57 original IOP disc reads**, **117 logical VBlanks**, **17 Timer 2 overflows**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 601 distinct entered / 595 completed; kernel 2 / 2; 92,392 call events / 92,336 returns |
| Mapping | 8,702 provisional EE entries; 8,705 raw generated files; 530 extra entries selected |
| IOP | 203,146 instructions; 284 physical entry targets; 15 load paths (embedded module reloaded) / 13 resident modules |
| Memory / threads | Nine IOP heap allocations / two frees (two sound-bank blocks at captured placements); four EE startup pools; main priority 2 → 24 admitted; 117 logical VBLANK_START deliveries (+2 polled); 234 owned helper wakes/resumes; logical Timer 2 with 16 overflow interrupts |
| Disc | Directory read plus 56 archive reads (57 total) through original CDVDFSV handlers, including main's first synchronous file load and a 52-sector streamed read (4 ring chunks, bytes equal the ISO); first AFS table (0x6880 bytes) byte-identical to original |
| Sound | 42 CRI calls (all five second-channel pairs); three EE→IOP sound uploads; two sound-driver commands through the original handlers with owned SPU2 voice transfers (DMA7 + IRQ 40); IOP sound block and bank blocks match the original; SOUNDS worker started with its IOP hardware timer; first 2,048-byte AutoDMA half programmed; two EE→IOP CRI stream-buffer copies; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Five packets / four clear draws / 1,146,880 sprite pixels; startup clear repeated; display setup (512×448 double buffer, VSync callback) matches original; no presented frame or VU execution |

Previous normal checkpoint → current: distinct EE **595 / 587 → 601 / 595**, calls/returns **92,247 / 92,184 → 92,392 / 92,336**, IOP instructions **198,954 → 203,146**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified this pass: serialized MSVC entry/ALL builds, **17/17 CTests**, **168 capture tests**, **57 recompilation tests**, **13 IOP tooling tests**, **22 normal-startup comparisons** (new sound-bank comparison) and the separate sound-worker diagnostic. Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Enabling VBLANK_START delivers the latched interrupt at once; original captures place it after the enable and before the request. Handlers run with the kernel ABI on the captured interrupt stack; iWakeupThread readies sleeping helpers 5/6, which preempt main and re-park. Main's request poll then uses one logical VBlank per idle call. Helper 6 drives `zs2us_1.afs` through original CDVDFSV readiness, read and error-query handlers. Main leaves the poll with the loaded 0x6880-byte table identical to the original capture (original: 120 VBlanks; timing not equated). The first R5900 FPU operations now use authored round-toward-zero, non-IEEE semantics.

All five late CRI `0x408` pairs complete through the original IOP handler; their objects match the original. Main then repeats its startup clear routine: GS reset, VSync handler removal, a polled VSync, both GIF clears against the original packets. It samples the field, reloads the embedded IOP module at the original heap placement and sets up the 512×448 display with a re-registered VSync callback. Double-buffer environment, field sample and GS library state match the original. EE Timer 2 now counts in logical time (one NTSC field per VBlank); each wrap runs the original overflow handler, and the timer library state matches the original. Main seeds its random state and loads its first file synchronously through original disc reads. Reads above 16 sectors now stream through the original FSV handler's 16-sector ring: each chunk restarts CDVD DMA channel 3 (retail block layout) and raises its own callback. Helper 6's 52-sector read completes in four chunks and its EE bytes equal the ISO. Main's post-load setup then reads two more files, uploads a 0x16640-byte sound block into the IOP sound buffer (one SIF DMA; bytes verified) and sends sound-driver command `0x80D0`. Its LIBSD voice transfers into SPU2 sound RAM now run through an owned DMA7 model with the original IRQ 40 handler ([SPU2 plan](IOP_SPU2_TRANSFER_PLAN.md)); two sound-driver commands complete. The loader then places two bank blocks at the original IOP heap placements and uploads them (bytes equal the original). Its final SOUNDS command `0x20` starts the SOUNDS worker and its 240 Hz IOP hardware timer, which now ticks in logical time ([timer plan](IOP_HARDWARE_TIMER_PLAN.md)). Main then enters its frame loop. The first frame step opens `cdrom0:\BIN\DBZP.BIN` through the C library's file API; stdio initialization binds boot FILEIO's file server and its first request runs through the original handler. The open, two seeks, the read and the close then run through FILEIO's per-file worker thread, and each completes through an IOP→EE SIF command into the C library's handler ([EE file I/O plan](EE_FILEIO_PLAN.md)). The loaded `DBZP.BIN` equals the original (`rpc-dbzp-loaded`). Main then calls into it: next, [EE overlay plan](EE_OVERLAY_PLAN.md). The IOP stream consumer acknowledgement is still unmodelled. See the [resume handoff](progress/ACTIVE_PASS.md).

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
