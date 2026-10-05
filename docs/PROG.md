# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Current status

The optional EE probe executes original generated BT2 code through platform startup, fourteen module-load paths (thirteen resident IOP modules), two owned IOP heap allocations, bounded sound/DBCMAN initialization and the initial MCSERV exchanges. The absent-card query now completes through real native IOP code, timed waits, secondary SIF transfer and generated EE callback/cleanup. The generated filesystem path now completes the first disc-search bind and original CDVDFSV metadata handler, its 36-byte transfer, and generated EE copy/cleanup. **No game assets loaded, displayed frame or native audio playback.**

Verified: MSVC builds, **15/15 CTests**, **116/116 capture tests**, **46/46 recompilation tests**, **13/13 IOP tooling tests**, and ten independent evidence validators. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

Execution now reaches a new **CDVDFSV command-service bind to 0x80000595**, RPC ID **44**, client **0x369A90**. The checked stop is CreateSema **selector 0x40 / PC 0x32EC68**, return **0x332814**. The [bounded disc-search scope](DISC_SEARCH_PLAN.md) completes the first lookup of `\DATA\ZS2US.DIR;1`: **LBA 0xE2A, size 64, result 1**, one actual native 36-byte secondary copy and generated cleanup. Three following reached entries are linked. The command/sector-read subsystem needs a separate request, destination and completion design; no successful sector or file-content read is inferred.

The [timed query scope](IOP_TIMED_WAIT_PLAN.md) completes five 300-us delays, priority dispatch, five FIFO probes, five alarm registrations / four real callbacks, an owned **192-byte secondary copy / one poll**, primary result **-11**, and generated **FUN_0031DD60** completion. Independent isolated captures with both cards disabled verify the result and transport.

Remaining ownership: the SDRDRV helper is parked on a pending reverse EE bind to **0x80000704** (EE server not registered). Logical IOP time is **1900 us**; the fifth card alarm remains armed at **2000 us**. General cross-CPU scheduling, later timer delivery, mounted cards, sector reads and sound playback remain incomplete. An EE child is ready but has not executed.

## Key counts

- **8,581** provisional EE map entries / **8,584** raw generated C++/header files; boundaries remain unverified. **Three additional reached entry bindings** linked this pass; these are not three restored gameplay functions.
- Latest full checkpoint: **1,463 EE calls / 1,452 returns**, **41,861 native IOP instructions / 164 physical entry targets / twelve ordinary waiting workers plus the parked reverse-bind helper**.
- **111 distinct EE functions entered / 107 completed** at the earlier unique-count checkpoint; updated distinct totals have not been measured. Call totals are execution events, not working-function counts.
- **Fourteen module-load paths / thirteen resident modules**, two captured IOP allocations, four EE startup pools, four DBCMAN binds/commands, and completed initial sound/MCSERV exchanges. Module discovery/relocation counts are in the history.
- Card query: **six DMA pairs / five timed FIFO probes**. No mounted-card filesystem support.
- **Three GS packets / two startup draws / 573,440 sprite pixels** processed. No presented game frame or VU microprogram execution.

## Milestones

- [x] Native host executable.
- [x] Local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools, provisional EE map and local generation.
- [x] Generated routines compiled/linked/executed; game entry reached.
- [x] Local kernel capture and bounded generated platform initialization.
- [x] Initial absent-card query through native IOP and generated EE callback.
- [x] First owned disc metadata lookup through original native IOP handler and generated EE cleanup.
- [ ] Complete startup and general required runtime integration.
- [ ] First rendered/presented frame; title screen and main menu.
- [ ] Character select and battle load.
- [ ] Playable versus battle.
- [ ] Audio and full match.

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
