# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute initialization, nested startup, semaphore/syscall setup and kernel scans. With explicit kernel and Timer 3 captures, the generated handler completes a bounded kernel patch and a main-RAM copy, completes FlushCache operations `0`/`2`, executes the separately generated copied-kernel entry and applies four more syscall registrations. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, four CTests, 26 recomp tooling tests and five capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops in `FUN_00338998` at SetSyscall (`0x74`), requesting extended selector `0x12C` with handler `0x80076488`, resume PC `0x003389A0`. The generated `FUN_80076000` lookup entry returned five times; syscall slots `0xFC` through `0xFF` were registered from its table. Extended selector `0x12C` is outside the current supported registration range and was not installed.

Next: establish the required extended-registration semantics and implement only the observed compatibility behavior, then rerun. Before each kernel dispatch, all copied payload bytes must match the compiled binding. Broader kernel entries, aliases, timer scheduling and BIOS exception behavior remain unverified.

## Key counts

- **7,244** functions generated locally: 7,243 main ELF + one scoped kernel entry; **7,250** raw generated C++/header files.
- **15** distinct functions entered across native probes.
- **12** functions completed across probes; entry, `FUN_00338520` and `FUN_00338A00` remain partial. These are startup/helper paths, not working gameplay.

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
