# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute generated entry initialization, RAM clearing, nested startup calls, semaphore creation and syscall registration. The generated kernel scan completes against a local PCSX2 capture and returns `0x80014F0C`, confirmed by an independent aligned scan. With the explicit snapshot, both scans return and `FUN_00338410` completes; `FUN_00338A00` is now compiled/registered and reaches an unsupported Timer 3 mode read. Game boot and gameplay are incomplete.

Latest verification: MSVC build/pipeline, four CTests, 20 recomp tooling tests and four disc-validator tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Snapshot-driven startup stops in `FUN_00338A00`: word read at `0x10001810` (Timer 3 mode), PC `0x00338A1C`. Generated `Load32` now forwards to checked memory access; MMIO remains unsupported. Without a snapshot, the original kernel-read boundary stays checked.

Next: establish Timer 3 mode state/behavior from pinned PS2SDK/PCSX2 evidence and local comparison, then add the smallest required MMIO support. Kernel snapshots remain read-only; full BIOS/timer behavior and game boot remain unverified.

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
