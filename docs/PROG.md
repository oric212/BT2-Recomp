# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute generated entry initialization, RAM clearing, nested startup calls, semaphore creation and syscall registration. The generated kernel scan completes against a local PCSX2 capture and returns `0x80014F0C`, confirmed by an independent aligned scan. Game boot and gameplay are incomplete.

Latest verification: MSVC build/pipeline, four CTests and 22 Python tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; trusted retail revision confirmation is pending.

## Current blocker and next step

Full startup stops in `FUN_00338388` at its first word read from `0x80000000`, PC `0x00338388`. The entry probe does not yet load the captured kernel snapshot; isolated scan execution succeeds with it.

Next: explicitly load the local snapshot into the bounded entry probe, execute to the next observed blocker, then implement only the required kernel table/patch access. The capture uses an instrumented standalone ELF boot with an active IOP; it does not validate BIOS exception behavior or full game boot.

## Key counts

- **7,242** functions generated locally; **7,245** generated C++/header files.
- **9** distinct functions entered across native probes.
- **5** functions completed across probes: four startup/isolated helpers plus the kernel scan on captured data. These are startup/helper paths, not working gameplay.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation and ELF metadata inspection.
- [ ] Trusted retail revision confirmation.
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
