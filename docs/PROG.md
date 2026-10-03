# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-03.

## Current status

The C++20 host and automated provisional map/recompilation pipeline work locally. Native probes execute generated entry/startup, kernel scans/copies/lookup, cache requests, buffer/list initialization, INTC registration/enable and Timer 2 setup. Explicit validated PCSX2 checkpoints support bounded continuation. Timer 2's second mode read matches its recorded startup write; the next initialization creates a third semaphore and a dormant thread, queries the main thread ID and changes its priority to 1. Explicit local OSD checkpoints support the first read, two ordered same-value setters and a validated second read. Generated OSD helpers return and the next startup routine registers another syscall handler. Focused discovery/generation now lets that handler execute to its first kernel store. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 31 recomp tooling tests and ten capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Generated `FUN_0033A7C8` stops at its first 32-bit store to `0x80075000`, PC `0x0033A7E8`. Startup requested a `0x330`-byte copy from `0x0036BAF8` through registered syscall `0x5A`. Handler discovery, generation and native dispatch work; the new kernel destination has no writable mapping. No bytes from this copy have been stored.

Next: add a bounded overlay for the observed destination/payload, preserve the existing captured kernel baseline and compiled binding checks, then rerun. Do not enable general kernel writes. Counter progression, interrupt delivery and scheduling remain unimplemented.

## Key counts

- **7,245** functions generated locally: 7,244 main ELF + one scoped kernel entry; **7,251** raw generated C++/header files.
- **39** distinct functions entered across native probes.
- **35** functions completed across probes; entry, `FUN_00338520`, `FUN_0033A820` and `FUN_0033A7B8` remain partial; `FUN_0033A7C8` completed in isolated RAM tests but remains partial during startup. These are startup/helper paths, not working gameplay.

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
