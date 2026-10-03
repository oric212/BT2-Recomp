# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute generated entry initialization, RAM clearing, nested startup calls, semaphore creation and syscall registration. The generated kernel scan completes against a local PCSX2 capture and returns `0x80014F0C`, confirmed by an independent aligned scan. With the explicit snapshot, both scans return and `FUN_00338410` completes; A local capture establishes Timer 3 mode `0x83` at the reached boundary; explicit single-read replay advances `FUN_00338A00` to an unregistered call to `FUN_00338998`. Game boot and gameplay are incomplete.

Latest verification: MSVC build/pipeline, four CTests, 22 recomp tooling tests and five capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

With explicit kernel and Timer 3 captures, startup stops at unregistered `FUN_00338998`: target `0x00338998`, source `0x00338A40`, resume `0x00338A48`. This target is already generated. Kernel-only mode still stops at Timer 3 MMIO; no-snapshot mode still stops at the first kernel read.

Next: compile/link/register `FUN_00338998` and execute to the next actual blocker. Timer support currently replays one supplied captured mode read; scheduling, interrupts, writes and repeated reads remain unsupported. Game boot is incomplete.

## Key counts

- **7,242** functions generated locally; **7,245** generated C++/header files.
- **10** distinct functions entered across native probes.
- **7** functions completed across probes; entry, `FUN_00338520` and `FUN_00338A00` remain partial. These are startup/helper paths, not working gameplay.

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
