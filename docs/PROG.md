# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated provisional map/recompilation pipeline work locally. Generated startup replays strictly validated local PCSX2 checkpoints. The first SIF register query returns, and generated code prepares a DMA descriptor and completes a checked cache-line path. Execution reaches the first SIF DMA submission. DMA transfers, interrupts, game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 38 recomp tooling tests and thirteen capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Generated `FUN_0032F020` stops at SifSetDma (`0x77`), resume PC `0x0032F028`. It submits one descriptor: source `0x003A9580`, destination `0x00019600`, size 20, flags `0x44`.

Next: establish the required SIF DMA/IOP receive behavior using reference evidence and a local checkpoint. A fabricated success return would leave the receiving subsystem unimplemented.

## Key counts

- **7,262** functions generated locally: 7,260 main ELF + two scoped kernel entries; **7,271** raw generated C++/header files.
- **60** distinct functions entered across native probes.
- **53** functions completed across probes; entry, `FUN_00100388`, `FUN_00331FE8`, `FUN_00331888` `FUN_00331D70`, `FUN_00331C38` and `FUN_0032F020` remain partial. These are startup/helper paths, not working gameplay.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools and synthetic translation.
- [x] Provisional BT2 function map and local C++ generation.
- [x] Generated routines compiled, linked and executed; game entry reached.
- [x] Local kernel capture and isolated generated scan validation.
- [x] Bounded generated platform initialization returned to entry.
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
