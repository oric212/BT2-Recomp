# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Verified checkpoint

<!-- progress-summary:start -->
![BT2 component status map](progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Native host, retail validation and recompilation pipeline | ✅ Verified |
| Generated BT2 startup execution | 🔧 Partial — reaches a checked runtime boundary |
| Initial disc read and metadata lookups | ✅ Verified |
| Complete startup and thread scheduling | 🔧 In progress — queued sound bind verified; priority handoff pending |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** main `ChangeThreadPriority(1, 24)` at EE PC `0x32EAF8`.

**Last measured, 2026-10-06:** **302 EE entries entered / 295 completed**, **37 CRI calls**, **six IOP heap allocations**. Complete startup is not yet verified.

**Separate helper diagnostic:** **312 / 298** EE coverage; reaches thread 5's original priority request. These counts are excluded from the normal checkpoint.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 302 distinct entered / 295 completed; kernel 2 / 2; 3,903 call events / 3,897 returns |
| Mapping | 8,585 provisional EE entries; 8,588 raw generated files; 242 extra entries selected |
| IOP | 81,640 instructions; 214 physical entry targets; 14 load paths / 13 resident modules |
| Memory / threads | Six IOP heap allocations; four EE startup pools; helper starts/priorities 4/4, suspensions 2; sound child parked; helpers 5/6 ready, unexecuted |
| Disc | One two-sector read; original completion callback; six metadata lookups/transfers; no archive payload decoded |
| Sound | 37 CRI calls; first 2,048-byte AutoDMA half programmed; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Three packets / two startup draws / 573,440 sprite pixels; startup clears only; no presented frame or VU execution |

Previous normal checkpoint → current: distinct EE **296 / 292 → 302 / 295**, calls/returns **3,892 / 3,887 → 3,903 / 3,897**, IOP instructions **61,612 → 81,640**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified this pass: serialized MSVC entry/ALL builds, **17/17 CTests**, **153 capture tests**, **55 recompilation tests**, **13 IOP tooling tests**, **16 existing original/native startup comparisons**, queued sound-bind comparison and two separate diagnostic comparisons. Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Normal startup stops at **ChangeThreadPriority(1, 24), selector 0x29 / PC 0x32EAF8**. Main priority remains 2; its request is not yet acknowledged.

The SOUNDS bind now queues at submission. Main's actual WaitSema dispatches the generated sound child on a retained owned stack; its registration and first SleepThread are verified. Original RPC completion resumes main once. The IOP reverse bind subsequently completes and its helper parks at the captured first sleep.

The separate `--startup-workers` diagnostic reaches helper 5's **ChangeThreadPriority(5, 1)**. Original captures establish helper priorities 16/18, temporary priority 1 and restore requests. Next: implement bounded main priority admission and helper critical-priority ownership, then continue eligible helper execution. Provisional first-wait guards have not yet been reached natively. See [RPC scheduling plan](EE_RPC_SCHEDULING_PLAN.md) and [resume handoff](progress/ACTIVE_PASS.md).

Remaining: general scheduling/timer delivery, sound wakes and command processing, mounted cards, archive payload loading, audio consumption and presentation. Complete startup and a presented game frame remain unverified.

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
