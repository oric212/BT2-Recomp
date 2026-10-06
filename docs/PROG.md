# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Current status

The optional native EE probe completes bounded platform/module startup, initial sound/DBCMAN exchanges, the absent-card query, disc bind/readiness and **the first real sector read**. Original translated IOP code transfers 4096 local ISO bytes and runs its owned completion callback; generated EE callback/cleanup clears the busy flag. Original game code parses the 64-byte directory index, completes all five listed file metadata lookups and returns to the main entry continuation. **No archive contents decoded, presented game frame or native audio playback.**

Verified: MSVC builds, **16/16 CTests**, **126/126 capture tests**, **47/47 recompilation tests**, **13/13 IOP tooling tests**, and twelve independent evidence validators. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

The new subsystem boundary is a **CRI_ADXI RPC bind**, service **0x90000200**, client **0x355C70**, RPC ID **52**, at **selector 0x77 / PC 0x32F028**. The resident module owns a validated server/worker, but its command lifecycle is unsupported. No bind response or command success is substituted.

Next: capture the original bind and first command, then define request/buffer/completion ownership for the retained worker. See [CRI RPC scope](CRI_RPC_PLAN.md). The [disc scope](DISC_COMMAND_PLAN.md) now completes the directory sector read and all five file lookups; generic multi-request reads, streaming and full disc-driver residency remain incomplete.

Remaining ownership: the SDRDRV helper is parked on a reverse EE bind to **0x80000704**. Logical IOP time is **1900 us**; the fifth card alarm remains armed at **2000 us**. General cross-CPU scheduling, later timer delivery, mounted cards and sound playback remain incomplete. An EE child is ready but has not executed.

## Key counts

- **8,584** provisional EE map entries / **8,587** raw generated C++/header files; boundaries remain provisional. **43 additional reached EE entries linked this pass**; several initialization frames remain pending. This is not a count of fully restored gameplay functions.
- Latest full checkpoint: **2,337 EE calls / 2,326 returns**, **43,298 native IOP instructions / 170 physical entry targets / twelve ordinary waiting workers plus the parked reverse-bind helper**.
- Earlier distinct-function checkpoint: **111 entered / 107 completed**; current distinct totals have not been remeasured. Calls/returns are execution events.
- **Fourteen module-load paths / thirteen resident modules**, two owned IOP allocations, four EE startup pools and completed initial sound/MCSERV exchanges.
- Disc: **one two-sector read / one original IOP callback / four logical read DMA jobs**, plus **six metadata lookups/transfers** (index and five listed files). Captured physical polling counts/timing are not equated with native logical transport.
- Card: **six DMA pairs / five timed FIFO probes**; no mounted-card filesystem support.
- **Three GS packets / two startup draws / 573,440 sprite pixels**; no presented game frame or VU microprogram execution.

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

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
