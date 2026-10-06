# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Current status

The user-confirmed native startup probe completes **37 CRI calls and six IOP heap allocations**, including second-channel creation and command 0x40A. The first channel's sixteen constructor pairs remain complete. The first owned 2048-byte AutoDMA half is programmed and remains pending. No DMA consumption, completion IRQ, native audio or presented game frame is claimed. Initial directory-sector loading and all five listed metadata lookups remain complete.

The last completed pass passed MSVC builds and **16/16 CTests**. Current tooling checks passed **142 capture tests**, **52 recompilation tests** and **13 IOP tooling tests**. Later worker changes are still under verification; their final build/test result is pending. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

Latest user-confirmed run stops at **ReferThreadStatus, selector 0x30 / EE PC 0x32EB68**, querying the next startup worker. The fifth and sixth heap requests now pass this checkpoint.

Next: verify the bounded worker status/start/priority changes, then measure the resulting scheduling boundary. See [EE sound scheduling plan](EE_SOUND_SCHEDULING_PLAN.md) and [second-channel evidence](CRI_SECOND_CHANNEL_PLAN.md). Pending changes are not counted as working.

Remaining: SDRDRV reverse bind to 0x80000704, general scheduling/timer delivery, mounted cards, archive payload loading, audio consumption and presentation. Logical IOP time remains 1900 us; the fifth card alarm is armed for 2000 us. An EE child is ready but has not executed.

## Key counts

- **8,584 provisional EE map entries / 8,587 raw generated files**. **221 extra selected entries** in the measured checkpoint. Fully restored gameplay-function totals are unknown.
- Full checkpoint: **3,855 EE calls / 3,848 returns**, **61,612 IOP instructions / 213 physical entry targets**. These are execution events, not restored-function counts.
- Measured distinct coverage: **291 EE entered / 286 completed**, plus **2 kernel entered / 2 completed**. Counts are unique addresses with observed entries/returns; they do not establish complete function correctness. The earlier 111 / 107 snapshot is preserved in history.
- Fourteen module-load paths / thirteen resident modules; **six owned IOP heap allocations**, four EE startup pools; CRI child parked on its first semaphore.
- Disc: one two-sector read, original completion callback, four logical read DMA jobs and six metadata lookups/transfers. No archive payload decoded.
- Card: six DMA pairs / five timed FIFO probes; no mounted-card filesystem support.
- GS: three packets / two startup draws / 573,440 sprite pixels; no presented frame or VU microprogram execution.

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

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
