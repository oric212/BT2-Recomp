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
| Complete startup and thread scheduling | 🔧 In progress — priority admission and helper first waits verified; interrupts/wakes pending |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** main's asynchronous load poll; first idle call `0x1007F0 → 0x2EF218` needs VBlank delivery and thread wakes.

**Last measured, 2026-10-06:** **429 EE entries entered / 420 completed**, **37 CRI calls**, **six IOP heap allocations**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 429 distinct entered / 420 completed; kernel 2 / 2; 4,729 call events / 4,723 returns |
| Mapping | 8,687 provisional EE entries; 8,690 raw generated files; 371 extra entries selected |
| IOP | 81,640 instructions; 214 physical entry targets; 14 load paths / 13 resident modules |
| Memory / threads | Six IOP heap allocations; four EE startup pools; main priority 2 → 24 admitted; helpers 5/6 run to first sleep; 28 balanced critical sections; VBlank handler registered, no delivery |
| Disc | One two-sector read; original completion callback; six metadata lookups/transfers; no archive payload decoded |
| Sound | 37 CRI calls; first 2,048-byte AutoDMA half programmed; two EE→IOP CRI stream-buffer copies; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Three packets / two startup draws / 573,440 sprite pixels; startup clears only; no presented frame or VU execution |

Previous normal checkpoint → current: distinct EE **302 / 295 → 429 / 420**, calls/returns **3,903 / 3,897 → 4,729 / 4,723**, IOP instructions unchanged at **81,640**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified this pass: serialized MSVC entry/ALL builds, **17/17 CTests**, **157 capture tests**, **57 recompilation tests**, **13 IOP tooling tests**, **17 normal-startup comparisons** (including the new helper comparison against nine original captures) and the separate sound-worker diagnostic. Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Main's ChangeThreadPriority(1, 24) is now admitted and returns 2. Helpers 5 (priority 16) and 6 (18) run first. Each takes the shared critical section ten times (temporary priority 1, resume/suspend of helper 4, restore), issues CRI stream-buffer copies to the IOP and parks at its captured first SleepThread. Main then registers and enables a VBLANK_START handler and starts an asynchronous request.

Normal startup stops at main's first idle call in the request poll loop (`0x1007F0 → 0x2EF218`). Progress requires interrupt delivery, owned thread wakes and the handle state machine; none is modeled. An unbounded run made no IOP request. Follow the [interrupt/load plan](EE_INTERRUPT_LOAD_PLAN.md); see the [resume handoff](progress/ACTIVE_PASS.md).

Remaining: interrupt/timer delivery, sleeping-thread wakes, sound command processing, mounted cards, archive payload loading, audio consumption and presentation. Complete startup and a presented game frame remain unverified.

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
