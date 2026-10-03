# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host builds and runs. Automated provisional mapping and PS2Recomp generation work locally. Native probes execute initialization, nested startup, semaphore/syscall setup and kernel scans. With explicit kernel and Timer 3 captures, the generated handler completes a bounded kernel patch and a main-RAM copy, completes FlushCache operations `0`/`2`, registers the copied kernel handler, then reaches its uncompiled callback. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, four CTests, 23 recomp tooling tests and five capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops in `FUN_003389F0` at syscall `0x5B`, dispatching to copied kernel handler `0x80076000`, resume PC `0x003389F8`. Registration returns, but the kernel target has no compiled native function and has not executed. The `0x740`-byte kernel overlay and `0x28`-byte main-RAM copy remain fully verified against their sources; the captured baseline is unchanged.

Next: create a separate relocation-aware map/generation path for the execution-blocking copied kernel payload. Original ELF discovery must not treat the kernel address as original `.text`. Only the observed KSEG0 patch range is writable; broader aliases, kernel execution, timer scheduling and BIOS exception behavior remain unverified.

## Key counts

- **7,243** functions generated locally; **7,246** generated C++/header files.
- **14** distinct functions entered across native probes.
- **10** functions completed across probes; entry, `FUN_00338520`, `FUN_00338A00` and `FUN_003389F0` remain partial. These are startup/helper paths, not working gameplay.

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
