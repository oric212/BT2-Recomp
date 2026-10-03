# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute initialization, nested startup, semaphore/syscall setup and kernel scans. With explicit kernel and Timer 3 captures, the generated handler completes a bounded kernel patch and a main-RAM copy, completes FlushCache operations `0`/`2`, executes the copied-kernel entry, records the Timer 3 interrupt registration and returns from kernel initialization. The next startup path fills a buffer and runs a generated list helper. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, five CTests, 26 recomp tooling tests and five capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops in `FUN_0032E950` at AddIntcHandler (`0x10`), cause `11`, handler `0x00339118`, resume PC `0x0032E958`. The `0x12C` Timer 3 registration now records a bounded handler/enable request; it does not deliver interrupts. The generated kernel lookup entry returned six times and `FUN_00338A00` completed.

Next: establish AddIntcHandler's required registration/return behavior and implement the observed path, then rerun. Initial zeroing is checked at SetupThread before later memory initialization. Kernel payload guards, buffer values/links, stack and registration state are verified; timer scheduling and BIOS interrupt execution remain unimplemented.

## Key counts

- **7,244** functions generated locally: 7,243 main ELF + one scoped kernel entry; **7,250** raw generated C++/header files.
- **19** distinct functions entered across native probes.
- **15** functions completed across probes; entry, `FUN_00338520`, `FUN_00338B78` and `FUN_0032E950` remain partial. These are startup/helper paths, not working gameplay.

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
