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
| Complete startup and thread scheduling | 🔧 In progress — VBlank delivery and helper wakes verified; IOP stream acknowledgement pending |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** generated entry `0x2FFA00` after the first late CRI `0x408` pair (38th CRI call).

**Last measured, 2026-10-07:** **511 EE entries entered / 502 completed**, **38 CRI calls**, **53 original IOP disc reads**, **105 logical VBlanks**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 511 distinct entered / 502 completed; kernel 2 / 2; 74,419 call events / 74,411 returns |
| Mapping | 8,699 provisional EE entries; 8,702 raw generated files; 441 extra entries selected |
| IOP | 141,526 instructions; 247 physical entry targets; 14 load paths / 13 resident modules |
| Memory / threads | Six IOP heap allocations; four EE startup pools; main priority 2 → 24 admitted; 105 logical VBLANK_START deliveries; 210 owned helper wakes/resumes |
| Disc | Directory read plus 52 archive reads (53 total), 53 readiness queries and 52 error queries through original CDVDFSV handlers; first AFS table (0x6880 bytes) byte-identical to original |
| Sound | 38 CRI calls (first late second-channel pair); first 2,048-byte AutoDMA half programmed; two EE→IOP CRI stream-buffer copies; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Three packets / two startup draws / 573,440 sprite pixels; startup clears only; no presented frame or VU execution |

Previous normal checkpoint → current: distinct EE **510 / 499 → 511 / 502**, calls/returns **74,373 / 74,359 → 74,419 / 74,411**, IOP instructions **138,268 → 141,526**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified this pass: serialized MSVC entry/ALL builds, **17/17 CTests**, **157 capture tests**, **57 recompilation tests**, **13 IOP tooling tests**, **19 normal-startup comparisons** (including the new VBlank/load comparison against three new original captures) and the separate sound-worker diagnostic. Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Enabling VBLANK_START delivers the latched interrupt at once; original captures place it after the enable and before the request. Handlers run with the kernel ABI on the captured interrupt stack; iWakeupThread readies sleeping helpers 5/6, which preempt main and re-park. Main's request poll then uses one logical VBlank per idle call. Helper 6 drives `zs2us_1.afs` through original CDVDFSV readiness, read and error-query handlers. Main leaves the poll with the loaded 0x6880-byte table identical to the original capture (original: 120 VBlanks; timing not equated). The first R5900 FPU operations now use authored round-toward-zero, non-IEEE semantics.

The first late CRI `0x408` pair now completes through the original IOP handler, validated against the CRI client's live RPC ID: disc RPCs consume IDs in between. Normal startup stops at mapped generated entry `0x2FFA00`, which is not yet selected. Next: select it and continue the remaining four pairs. The IOP stream consumer acknowledgement (original trailer counter 2) is not modeled. See [interrupt/load plan](EE_INTERRUPT_LOAD_PLAN.md) and the [resume handoff](progress/ACTIVE_PASS.md).

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
