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
| Game asset loading and first presented frame | 🔧 In progress — first overlay asset read completes; frame 2 reaches its first texture upload; nothing presented |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** the overlay's second frame starts its first VIF1 chain with content (PATH2 DIRECT: a PSMT4 texture, its CLUT and one textured sprite), held as a checked boundary at the channel start ([GS texture plan](GS_TEXTURE_PLAN.md)).

**Last measured, 2026-10-07:** **642 EE entries entered / 633 completed**, **42 CRI calls**, **58 original IOP disc reads**, **123 logical VBlanks**, **18 Timer 2 overflows**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.

<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 642 distinct entered / 633 completed (4 overlay entries selected); kernel 2 / 2; 97,963 call events / 97,899 returns |
| Mapping | 9,405 provisional EE entries (152 quarantined); 9,408 raw generated files; 567 extra entries selected; overlay map 425 entries, 4 selected |
| IOP | 229,661 instructions; SPU2 voice positions/ENDX from owned ADPCM block flags (no samples/ADSR); 24 logical SOUNDS timer ticks; 284 physical entry targets; 15 load paths (embedded module reloaded) / 13 resident modules |
| Memory / threads | Nine IOP heap allocations / two frees (two sound-bank blocks at captured placements); four EE startup pools; main priority 2 → 24 admitted; 117 logical VBLANK_START deliveries (+2 polled); 234 owned helper wakes/resumes; logical Timer 2 with 16 overflow interrupts |
| Disc | Directory read plus 56 archive reads (57 total) through original CDVDFSV handlers, including main's first synchronous file load and a 52-sector streamed read (4 ring chunks, bytes equal the ISO); first AFS table (0x6880 bytes) byte-identical to original |
| Sound | 42 CRI calls (all five second-channel pairs); three EE→IOP sound uploads; two sound-driver commands through the original handlers with owned SPU2 voice transfers (DMA7 + IRQ 40); IOP sound block and bank blocks match the original; SOUNDS worker started with its IOP hardware timer; first 2,048-byte AutoDMA half programmed; two EE→IOP CRI stream-buffer copies; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Five packets / four clear draws / 1,146,880 sprite pixels; startup clear repeated; display setup (512×448 double buffer, VSync callback) matches original; no presented frame or VU execution |

Previous normal checkpoint → current: distinct EE **620 / 609 → 642 / 633**, calls/returns **92,419 / 92,358 → 97,963 / 97,899**, IOP instructions **203,146 → 229,661**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified this pass: serialized MSVC entry/ALL builds, **17/17 CTests**, **175 capture**, **63 recompilation**, **13 IOP** and 3 Ghidra Python tests, **26 normal-startup comparisons**, four phase-matched comparisons (`BT2_PHASE_STOP`) and the unchanged sound-worker diagnostic (302/295). Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Enabling VBLANK_START delivers the latched interrupt at once; original captures place it after the enable and before the request. Handlers run with the kernel ABI on the captured interrupt stack; iWakeupThread readies sleeping helpers 5/6, which preempt main and re-park. Main's request poll then uses one logical VBlank per idle call. Helper 6 drives `zs2us_1.afs` through original CDVDFSV readiness, read and error-query handlers. Main leaves the poll with the loaded 0x6880-byte table identical to the original capture (original: 120 VBlanks; timing not equated). The first R5900 FPU operations now use authored round-toward-zero, non-IEEE semantics.

All five late CRI `0x408` pairs complete through the original IOP handler; their objects match the original. Main then repeats its startup clear routine: GS reset, VSync handler removal, a polled VSync, both GIF clears against the original packets. It samples the field, reloads the embedded IOP module at the original heap placement and sets up the 512×448 display with a re-registered VSync callback. Double-buffer environment, field sample and GS library state match the original. EE Timer 2 now counts in logical time (one NTSC field per VBlank); each wrap runs the original overflow handler, and the timer library state matches the original. Main seeds its random state and loads its first file synchronously through original disc reads. Reads above 16 sectors now stream through the original FSV handler's 16-sector ring: each chunk restarts CDVD DMA channel 3 (retail block layout) and raises its own callback. Helper 6's 52-sector read completes in four chunks and its EE bytes equal the ISO. Main's post-load setup then reads two more files, uploads a 0x16640-byte sound block into the IOP sound buffer (one SIF DMA; bytes verified) and sends sound-driver command `0x80D0`. Its LIBSD voice transfers into SPU2 sound RAM now run through an owned DMA7 model with the original IRQ 40 handler ([SPU2 plan](IOP_SPU2_TRANSFER_PLAN.md)); two sound-driver commands complete. The loader then places two bank blocks at the original IOP heap placements and uploads them (bytes equal the original). Its final SOUNDS command `0x20` starts the SOUNDS worker and its 240 Hz IOP hardware timer, which now ticks in logical time ([timer plan](IOP_HARDWARE_TIMER_PLAN.md)). Main then enters its frame loop. The first frame step opens `cdrom0:\BIN\DBZP.BIN` through the C library's file API; stdio initialization binds boot FILEIO's file server and its first request runs through the original handler. The open, two seeks, the read and the close then run through FILEIO's per-file worker thread, and each completes through an IOP→EE SIF command into the C library's handler ([EE file I/O plan](EE_FILEIO_PLAN.md)). The loaded `DBZP.BIN` equals the original (`rpc-dbzp-loaded`). Main then calls into it. The overlay is analysed and generated as a separate program; its functions run only while RAM holds the bytes they were generated from ([EE overlay plan](EE_OVERLAY_PLAN.md)). Its frame path starts Timer 1 for an H-BLANK delay and flips the display environment. The display block, the overlay range and the recorded GS display registers (512×448 interlaced) match the original (`rpc-overlay-dispenv`). Its first frame then runs through general frame GIF DMA ([frame GIF plan](FRAME_GIF_PLAN.md)):
- **Draw environment:** the send clears the whole 512×448 framebuffer in the owned VRAM model (229,376 pixels).
- **Path sync and render kick:** the GS library's path sync sees idle paths, and the game's render kick sends an empty VIF1 chain.
- **Display setup:** the game's own display writes configure both read circuits.

The frame's Timer 1 wait then crosses field boundaries: the IOP SOUNDS timer ticks in logical time, and its worker reads SPU2 ENDX from an owned voice position model ([SPU2 voice plan](SPU2_VOICE_PLAN.md)). The frame's one-shot VSync flag is published as in the original. The overlay then loads its first indexed asset through CRI's file layer: each status poll is a logical idle point, and the read completes through the original disc handlers. Two frame-phase controller-port requests run through the original DBC handlers. The second frame's render kick then carries the first real GS work (texture, CLUT, textured sprite), held at the VIF1 channel start: next, [GS texture plan](GS_TEXTURE_PLAN.md). The IOP stream consumer acknowledgement is still unmodelled. See the [resume handoff](progress/ACTIVE_PASS.md).

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
