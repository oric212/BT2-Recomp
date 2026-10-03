# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated map/recompilation pipeline work locally. Generated startup returns from its SIF initialization helper using explicit PCSX2 checkpoints and a bounded native CHANGE_SADDR receive model. The next path allocates a packet, creates a semaphore and prepares an RPC bind request. General DMA/IOP services, interrupts, complete game boot and gameplay remain incomplete.

Latest verification: MSVC build/pipeline, seven CTests, 40 recomp tooling tests and fifteen capture-tool tests passed. Game files, BIOS, captures and proprietary-derived output remain ignored. Function boundaries are provisional; the retail revision is confirmed against official Redump disc checksums and the embedded executable.

## Current blocker and next step

Generated `FUN_0032F020` stops at the second SifSetDma (`0x77`), resume PC `0x0032F028`. Pending RPC bind command `0x80000009` requests server ID `0x80000592`; descriptor source `0x203A97C0`, destination `0x00019600`, size 64, flags `0x44`.

Next: validate the IOP service lookup/response and required EE delivery behavior using reference evidence and local captures. The server's purpose remains unknown; no RPC response is fabricated.

## Key counts

- **7,262** functions generated locally: 7,260 main ELF + two scoped kernel entries; **7,271** raw generated C++/header files.
- **66** distinct functions entered across native probes.
- **62** functions completed across probes; entry, `FUN_00100388`, `FUN_0031F8B0` and `FUN_00332790` remain partial. Previously completed functions can be reached again and stop on new inputs. These are startup/helper paths, not working gameplay.

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
