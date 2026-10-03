# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated provisional map/recompilation pipeline work locally. Generated platform initialization now returns to the entry code after bounded kernel copies/lookup registrations, buffer setup, INTC/Timer 2 setup, dormant thread creation, priority adjustment and OSD calls. Explicit validated PCSX2 checkpoints support this continuation. Entry restores its stack, executes EI and enters generated `FUN_00100388`, and its first generated helper `FUN_00321E50`. The helper takes its initial branch path, stores a one-time flag and restores its frame before tail dispatch. Game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 35 recomp tooling tests and ten capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Generated `FUN_00321E50` stops at unlinked tail target `FUN_00321D90` from `0x00321E74`. Target range `[0x00321D90,0x00321E50)` is mapped and generated locally. The earlier gap is verified unreachable NOP padding and now accepted by a narrow source-checked map rule.

Next: link the reached target, execute, and inspect the actual next blocker. Counter progression, interrupt delivery and scheduling remain unimplemented.

## Key counts

- **7,262** functions generated locally: 7,260 main ELF + two scoped kernel entries; **7,271** raw generated C++/header files.
- **44** distinct functions entered across native probes.
- **41** functions completed across probes; entry, `FUN_00100388` and `FUN_00321E50` remain partial at tail dispatch. These are startup/helper paths, not working gameplay.

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
