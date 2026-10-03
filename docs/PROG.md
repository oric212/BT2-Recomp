# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated provisional map/recompilation pipeline work locally. Native probes execute generated entry/startup, kernel scans/copies/lookup, cache requests, buffer/list initialization, INTC registration/enable and Timer 2 setup. Explicit validated PCSX2 checkpoints support bounded continuation. Timer 2's second mode read matches its recorded startup write; the next initialization creates a third semaphore and a dormant thread, queries the main thread ID and changes its priority to 1. Explicit local OSD checkpoints support the first read, two ordered same-value setters and a validated second read. Generated OSD helpers return and the next startup routine registers another syscall handler. The generated handler now completes its second kernel payload copy into a bounded overlay; startup registers and executes the copied lookup entry with full payload checks. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 34 recomp tooling tests and ten capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Startup stops registering interior kernel entry `0x80075038` for selector `0x55`, via SetSyscall (`0x74`) at resume PC `0x0033A818`. Generated lookup `FUN_80075000` executed and returned that address. Registration currently permits only the copied payload entry, so this new address is rejected before registration or execution.

Next: validate registration of the returned interior entry and generate only its reached code when execution requires it. Preserve full payload checks, the captured baseline and earlier binding. Counter progression, interrupt delivery and scheduling remain unimplemented.

## Key counts

- **7,246** functions generated locally: 7,244 main ELF + two scoped kernel entries; **7,255** raw generated C++/header files.
- **41** distinct functions entered across native probes.
- **38** functions completed across probes; entry, `FUN_00338520` and `FUN_0033A820` remain partial. `FUN_0033A810` completed earlier invocations and is partial on the current registration call. These are startup/helper paths, not working gameplay.

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
