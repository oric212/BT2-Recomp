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
| Complete startup and thread scheduling | 🔧 In progress — latest worker changes unverified |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** worker `ReferThreadStatus` at EE PC `0x32EB68`.

**Last measured, 2026-10-06:** **291 EE entries entered / 286 completed**, **37 CRI calls**, **six IOP heap allocations**. Complete startup is not yet verified.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 291 distinct entered / 286 completed; kernel 2 / 2; 3,855 call events / 3,848 returns |
| Mapping | 8,584 provisional EE entries; 8,587 raw generated files; 221 extra entries selected |
| IOP | 61,612 instructions; 213 physical entry targets; 14 load paths / 13 resident modules |
| Memory / threads | Six IOP heap allocations; four EE startup pools; one child ready but unexecuted |
| Disc | One two-sector read; original completion callback; six metadata lookups/transfers; no archive payload decoded |
| Sound | 37 CRI calls; first 2,048-byte AutoDMA half programmed; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Three packets / two startup draws / 573,440 sprite pixels; startup clears only; no presented frame or VU execution |

Previous measured checkpoint → current: CRI calls **35 → 37**, heaps **4 → 6**, EE calls **3,415 → 3,855**, IOP instructions **61,054 → 61,612**. The older **111 / 107** distinct count is from an earlier checkpoint, not a same-pass delta. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

The last completed pass passed MSVC builds and **16/16 CTests**. Current tooling checks passed **142 capture tests**, **52 recompilation tests** and **13 IOP tooling tests**. Later worker changes are still under verification; their final build/test result is pending. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

Latest user-confirmed run stops at **ReferThreadStatus, selector 0x30 / EE PC 0x32EB68**, querying the next startup worker. The fifth and sixth heap requests now pass this checkpoint.

Next: verify the bounded worker status/start/priority changes, then measure the resulting scheduling boundary. See [EE sound scheduling plan](EE_SOUND_SCHEDULING_PLAN.md) and [second-channel evidence](CRI_SECOND_CHANNEL_PLAN.md). Pending changes are not counted as working.

Remaining: SDRDRV reverse bind to 0x80000704, general scheduling/timer delivery, mounted cards, archive payload loading, audio consumption and presentation. Logical IOP time remains 1900 us; the fifth card alarm is armed for 2000 us. An EE child is ready but has not executed.

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

See [active-pass handoff](progress/ACTIVE_PASS.md) for pending files, verification steps and resume instructions. See [tracking rules](PROGRESS_TRACKING.md) for status definitions and README synchronization. New changes enter the verified checkpoint only after their required checks pass.

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
