# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute initialization, nested startup, semaphore/syscall setup and kernel scans. With explicit kernel and Timer 3 captures, the generated handler completes a bounded kernel patch and a main-RAM copy, completes FlushCache operations `0`/`2`, executes the copied-kernel entry, records the Timer 3 interrupt registration and returns from kernel initialization. The next startup path fills a buffer, runs a generated list helper, registers INTC cause 11 as ID 2 and completes a generated CPU-status helper. Explicit Timer 2 capture replay and the observed mode write complete the generated register-write helper and its ErrorEPC/ERET return. With an explicit INTC checkpoint, EnableIntc and the timer startup routine return. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, six CTests, 28 recomp tooling tests and seven capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops in `FUN_00338DB8` at a second Timer 2 mode read (`0x10001010`, PC `0x00338DD8`). An isolated pre-EnableIntc capture observed mask `0x1002`; cause 11 enable returns `1`, records `0x1802`, and generated `FUN_0032E9A0`, `FUN_0032F7A8` and `FUN_00338B78` complete. Neither interrupt handler has executed.

Next: validate Timer 2 mode after its startup write and support the reached subsequent read, then rerun. The current one-read capture replay deliberately stops here; counter progression, event scheduling and interrupt delivery remain unimplemented. INTC state comes from the explicit checkpoint, not a fabricated BIOS default. Earlier capture-free boundaries and kernel/stack/registration guards stay checked.

## Key counts

- **7,244** functions generated locally: 7,243 main ELF + one scoped kernel entry; **7,250** raw generated C++/header files.
- **25** distinct functions entered across native probes.
- **22** functions completed across probes; entry, `FUN_00338520` and `FUN_00338DB8` remain partial. These are startup/helper paths, not working gameplay.

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
