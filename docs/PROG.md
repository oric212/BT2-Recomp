# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host and automated provisional map/recompilation pipeline work locally. Native probes execute generated entry/startup, kernel scans/copies/lookup, cache requests, buffer/list initialization, INTC registration/enable and Timer 2 setup. Explicit validated PCSX2 checkpoints support bounded continuation. Timer 2's second mode read matches its recorded startup write; the next initialization creates a third semaphore and a dormant thread, queries the main thread ID and changes its priority to 1. Explicit local OSD checkpoints support the first read, one same-value setter and a validated second read. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, six CTests, 31 recomp tooling tests and ten capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops at the second SetOsdConfigParam (`0x4A`) call in `FUN_0032ED00`, resume PC `0x0032ED08`, input pointer `0x01FFECE0`, restoring the saved original word. The first setter and second getter completed; local readback equals the original configuration. Only the observed same-value write is supported.

Next: validate and continue the restore call, then link the next reached generated path. Omitting OSD readback validation preserves the first setter stop. Counter progression, interrupt delivery and scheduling remain unimplemented.

## Key counts

- **7,244** functions generated locally: 7,243 main ELF + one scoped kernel entry; **7,250** raw generated C++/header files.
- **35** distinct functions entered across native probes.
- **31** functions completed across probes; entry, `FUN_00338520`, `FUN_00338648`, `FUN_003385E0` remain partial; `FUN_0032ED00` completed its first invocation and is partial on its second. These are startup/helper paths, not working gameplay.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools and synthetic translation.
- [x] Provisional BT2 function map and local C++ generation.
- [x] Generated routines compiled, linked and executed; game entry reached.
- [x] Local kernel capture and isolated generated scan validation.
- [ ] Full generated-code/runtime integration and complete startup.
- [ ] First rendered frame.
- [ ] Title screen and main menu.
- [ ] Character select and battle load.
- [ ] Playable versus battle.
- [ ] Audio and full match.

## History and references

- [October 2026 detailed history](progress/2026-10.md): all previous progress entries, evidence, counts and historical milestone snapshots preserved. Statements about "current" work inside that archive describe their original point in time.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [tool workflow](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

Keep this page limited to current status. Append detailed pass results to `progress/YYYY-MM.md`, linking new monthly logs here. Update the blocker, counts and checklist after verification.
