# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute entry initialization, nested startup, semaphore creation, syscall registration and kernel scans. Explicit replay of captured Timer 3 mode `0x83` advances startup through `FUN_00338998` and into the discovered registered handler `FUN_003389B8`. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, four CTests, 22 recomp tooling tests and five capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

With explicit kernel and Timer 3 captures, `FUN_003389B8` stops at its first rejected kernel word write: address `0x80076000`, PC `0x003389D8`. The handler read a source word from `0x0036B328`; its destination lies in the captured KSEG0 window. Kernel-only and no-snapshot stop paths remain checked.

Next: establish the required bounded kernel patch storage and write semantics, then continue the generated handler. Preserve the captured baseline and reject unrelated memory/MMIO. Timer support still replays one supplied read; full hardware behavior and game boot remain unverified.

## Key counts

- **7,243** functions generated locally; **7,246** generated C++/header files.
- **13** distinct functions entered across native probes.
- **8** functions completed across probes; five remain partial, including the new wrapper/handler path. These are startup/helper paths, not working gameplay.

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
