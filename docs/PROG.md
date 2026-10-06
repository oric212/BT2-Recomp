# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Current status

The optional native EE probe completes bounded platform/module startup, initial sound/DBCMAN exchanges, the absent-card query, disc bind/readiness and **the first real sector read**. Original translated IOP code transfers 4096 local ISO bytes and runs its owned completion callback; generated EE callback/cleanup clears the busy flag. Original game code parses the 64-byte directory index, completes all five listed file metadata lookups and returns to the main entry continuation. CRI bind and original command-2 stream-slot allocation also complete, with a third owned IOP heap block. **No archive contents decoded, presented game frame or native audio playback.**

Verified: MSVC builds, **16/16 CTests**, **130/130 capture tests**, **47/47 recompilation tests**, **13/13 IOP tooling tests**, and thirteen independent evidence validators. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

The new subsystem boundary is **EE cache maintenance**, target **0x32F558**, caller **0x2FCD84**, resume **0x2FCD8C**, with range arguments **0x365BC0 / 0x36643F**. Linking its helper exposes CACHE 0x10/0x14 and COP0 TagLo operations missing from the probe ABI. Unsupported cache routines remain unlinked; no successful cache effects are substituted.

Next: establish the reached cache/transfer contract and validate the original tag reads before enabling these routines. See [cache scope](EE_CACHE_TRANSFER_PLAN.md). [CRI RPC scope](CRI_RPC_PLAN.md) now completes bind, heap allocation and command 2 through generated EE cleanup; command 0x403 works in the isolated original-handler test, but full EE delivery remains behind the cache boundary. The next captured command is 0x40C; its effects remain unknown.

Remaining ownership: the SDRDRV helper is parked on a reverse EE bind to **0x80000704**. Logical IOP time is **1900 us**; the fifth card alarm remains armed at **2000 us**. General cross-CPU scheduling, later timer delivery, mounted cards and sound playback remain incomplete. An EE child is ready but has not executed.

## Key counts

- **8,584** provisional EE map entries / **8,587** raw generated C++/header files; boundaries remain provisional. **3 additional reached EE entries linked this pass**; several initialization frames remain pending. This is not a count of fully restored gameplay functions.
- Latest full checkpoint: **2,381 EE calls / 2,374 returns**, **43,416 native IOP instructions / 172 physical entry targets / twelve ordinary waiting workers plus the parked reverse-bind helper**.
- Earlier distinct-function checkpoint: **111 entered / 107 completed**; current distinct totals have not been remeasured. Calls/returns are execution events.
- **Fourteen module-load paths / thirteen resident modules**, three owned IOP allocations, four EE startup pools and completed initial sound/MCSERV exchanges.
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
