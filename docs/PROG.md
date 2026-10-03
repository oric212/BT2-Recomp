# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute initialization, nested startup, semaphore/syscall setup and kernel scans. With explicit kernel and Timer 3 captures, the generated handler completes a bounded kernel patch and a main-RAM copy, completes FlushCache operations `0`/`2`, executes the copied-kernel entry, records the Timer 3 interrupt registration and returns from kernel initialization. The next startup path fills a buffer, runs a generated list helper, registers INTC cause 11 as ID 2 and completes a generated CPU-status helper. Explicit Timer 2 capture replay and the observed mode write complete the generated register-write helper and its ErrorEPC/ERET return. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, six CTests, 27 recomp tooling tests and six capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops at EnableIntc syscall `0x14`, cause `11`, resume PC `0x0032E9A8` in `FUN_0032E9A0`, called by `FUN_0032F7A8`. The observed Timer 2 mode write `0x382` to `0xB0001010` now completes once after explicit read replay. Generated `FUN_00338B58` and `FUN_00338AD8` return through ErrorEPC/ERET; the probe verifies restored Status and cleared LL bit.

Next: establish the required EnableIntc return/mask behavior and implement the observed cause 11 request. Timer mode bits are recorded; counter progression, event scheduling and interrupt delivery remain unimplemented. Retail BIOS CPU status remains unconfirmed. Earlier capture-free boundaries and kernel/stack/registration guards stay checked.

## Key counts

- **7,244** functions generated locally: 7,243 main ELF + one scoped kernel entry; **7,250** raw generated C++/header files.
- **24** distinct functions entered across native probes.
- **19** functions completed across probes; entry, `FUN_00338520`, `FUN_00338B78`, `FUN_0032F7A8` and `FUN_0032E9A0` remain partial. These are startup/helper paths, not working gameplay.

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
