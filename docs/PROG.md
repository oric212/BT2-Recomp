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
| Complete startup and thread scheduling | 🔧 In progress — four helpers verified; queued RPC scheduling pending |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** main `ChangeThreadPriority(1, 24)` at EE PC `0x32EAF8`.

**Last measured, 2026-10-06:** **296 EE entries entered / 292 completed**, **37 CRI calls**, **six IOP heap allocations**. Complete startup is not yet verified.

**Separate diagnostic:** generated sound registration and EE/IOP first waits verified: **302 / 295** EE coverage, one real reverse-bind completion. Main has not resumed.
<!-- progress-summary:end -->

## Measured execution and subsystem evidence

| Area | Last verified measurement / scope |
| --- | --- |
| EE | 296 distinct entered / 292 completed; kernel 2 / 2; 3,892 call events / 3,887 returns |
| Mapping | 8,584 provisional EE entries; 8,587 raw generated files; 232 extra entries selected (six added for the diagnostic sound path) |
| IOP | 61,612 instructions; 213 physical entry targets; 14 load paths / 13 resident modules |
| Memory / threads | Six IOP heap allocations; four EE startup pools; helper starts/priorities 4/4, suspensions 2; sound child and helpers 5/6 ready, unexecuted |
| Disc | One two-sector read; original completion callback; six metadata lookups/transfers; no archive payload decoded |
| Sound | 37 CRI calls; first 2,048-byte AutoDMA half programmed; no consumption / completion IRQ / native playback verified |
| Cards | Six DMA pairs / five timed FIFO probes; absent-card path only |
| GS | Three packets / two startup draws / 573,440 sprite pixels; startup clears only; no presented frame or VU execution |

Previous measured checkpoint → current: distinct EE **291 / 286 → 296 / 292**; calls/returns **3,855 / 3,848 → 3,892 / 3,887**. Across the completed capacity/worker pass, CRI calls advanced **35 → 37**, heaps **4 → 6**, IOP instructions **61,054 → 61,612**, and selected extras **200 → 226**. Counts measure observed execution, not restored gameplay functions or overall completion.

The numeric snapshot is in [latest.json](progress/latest.json); authored component scopes are in [components.json](progress/components.json). Both retain the last measured checkpoint while pending work remains unverified.
## Verification

Verified: current MSVC entry/ALL builds, **17/17 CTests**, **146 capture tests**, **52 recompilation tests**, **13 IOP tooling tests**, **16 original/native startup comparisons**, the 22-row ownership layout and isolated second-channel service validation. Four worker contexts, their ready/suspended states and priorities match the captured scope. Five later CRI constructor pairs remain service-only coverage; the full EE checkpoint is still 37 calls. Proprietary inputs and generated output remain ignored.

## Current blocker and next step

Full native startup stops at **ChangeThreadPriority, selector 0x29 / EE PC 0x32EAF8**, requesting main priority **2 → 24**. The request is deliberately not acknowledged: ready thread 3 at priority 5 must execute before main continues.

A guarded diagnostic now executes the original child registration/loop on a retained stack, reaches its first SleepThread, completes the actual native IOP reverse bind and parks SDRDRV at its captured first sleep. Its EE stack is cancelled at the diagnostic boundary; no main resume or general scheduler is claimed. Original captures show registration while main binds SOUNDS (RPC ID 29), earlier than this diagnostic handoff. Replace immediate RPC completion with owned queued completion and dispatch at that earlier blocking point; see [queued RPC scheduling plan](EE_RPC_SCHEDULING_PLAN.md). See [EE sound scheduling plan](EE_SOUND_SCHEDULING_PLAN.md) and [resume handoff](progress/ACTIVE_PASS.md). Do not substitute a successful return for dispatch.

Remaining in normal startup: timely SDRDRV reverse bind to 0x80000704, general scheduling/timer delivery, mounted cards, archive payload loading, audio consumption and presentation. Logical IOP time remains 1900 us; the fifth card alarm is armed for 2000 us. The sound child and helpers 5/6 are ready but unexecuted; helpers 4/7 are suspended.

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
