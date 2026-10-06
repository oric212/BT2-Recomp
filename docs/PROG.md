# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Current status

Original translated CRI command 0x40C, the fourth IOP heap allocation and all **sixteen 0x422/0x400 constructor pairs** now complete through generated EE cleanup: **35 CRI calls completed**. The first owned 2048-byte AutoDMA half is programmed and remains pending. No DMA consumption, completion IRQ, native audio or presented game frame is claimed. Initial directory-sector loading and all five listed metadata lookups remain complete.

Verified: MSVC builds, **16/16 CTests**, **141 capture tests**, **50 recompilation tests**, **13 IOP tooling tests**, independent original AutoDMA/constructor evidence and three relocated module comparisons. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

At **selector 0x77 / EE PC 0x32F028**, the fifth heap request needs **0x8D0 bytes at captured pointer 0x191700**. Its rounded interval overlaps translated FILEIO. The request is not accepted. The original call-36 capture establishes the subsequent second-channel command 2.

Next: inventory later second-channel allocations and choose a consistent bounded IOP layout before admitting the fifth request. See [second-channel capacity plan](CRI_SECOND_CHANNEL_PLAN.md) and [first AutoDMA evidence](CRI_AUTODMA_PLAN.md). Moving another image without establishing capacity would be speculative.

Remaining: SDRDRV reverse bind to 0x80000704, general scheduling/timer delivery, mounted cards, archive payload loading, audio consumption and presentation. Logical IOP time remains 1900 us; the fifth card alarm is armed for 2000 us. An EE child is ready but has not executed.

## Key counts

- **8,584 provisional EE map entries / 8,587 raw generated files**. **10 more EE entries linked this pass**, 200 extra selected entries in total. Fully restored gameplay-function totals are unknown.
- Full checkpoint: **3,415 EE calls / 3,405 returns**, **61,054 IOP instructions / 213 physical entry targets**; **10,249 additional IOP instructions** this pass. These are execution events, not restored-function counts.
- Earlier distinct checkpoint: **111 entered / 107 completed**; current distinct totals have not been remeasured.
- Fourteen module-load paths / thirteen resident modules; **four owned IOP heap allocations**, four EE startup pools; CRI child parked on its first semaphore.
- Disc: one two-sector read, original completion callback, four logical read DMA jobs and six metadata lookups/transfers. No archive payload decoded.
- Card: six DMA pairs / five timed FIFO probes; no mounted-card filesystem support.
- GS: three packets / two startup draws / 573,440 sprite pixels; no presented frame or VU microprogram execution.

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
