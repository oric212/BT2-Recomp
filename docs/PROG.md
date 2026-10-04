# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated map/recompilation pipeline work locally. Generated startup completes SIF initialization and its first CD initialization RPC bind using explicit PCSX2 checkpoints and bounded logical receive/response models. The generated response callback preserves the caller CPU and scratch stack, and binding consumes its signal and deletes its temporary semaphore. The next RPC call prepares real input/output descriptors. General DMA/IOP services, hardware interrupts, complete game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 40 recomp tooling tests, 21 capture-tool tests and the isolated RPC_END probe passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Generated `FUN_0032F020` stops at the third SifSetDma (`0x77`), resume PC `0x0032F028`. The first CD initialization RPC call (command `0x8000000A`, function number 0) submits two descriptors: four bytes at `0x003A8800` to service buffer `0x0003FB20`, then a 64-byte packet at `0x203A97C0` to `0x00019600`. Requested reply is 16 bytes at `0x003695C0`.

Next: validate CD initialization service execution and reply effects against public source and local PCSX2 evidence. The bind completes natively; the service call remains unsupported. No CD status or disc result is fabricated.

## Key counts

- **7,262** functions generated locally: 7,260 main ELF + two scoped kernel entries; **7,271** raw generated C++/header files.
- **71** distinct functions entered across native probes.
- **67** functions completed across probes; entry, `FUN_00100388`, `FUN_0031F8B0` and `FUN_00332970` remain partial. This pass completed the RPC bind helper and its newly linked semaphore-delete wrapper; the RPC call helper is entered but partial. Previously completed functions can be reached again and stop on new inputs. These are startup/helper paths, not working gameplay.

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
