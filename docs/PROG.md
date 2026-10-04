# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated recompilation pipeline work locally. Checked startup submits the real IOP reboot request, clears four reset registers and returns through two startup helpers. The local IOPRP300 image matches the disc. Complete IOP boot, disc services and gameplay remain incomplete.

Latest verification: MSVC build/full checkpoint pipeline, seven CTests, 42 recomp-tool tests, 39 capture-tool tests and five IOP-inventory tests passed. Proprietary files, captures, generated code and automatic bindings remain ignored. Retail revision is confirmed; function boundaries remain provisional.

## Current blocker and next step

Post-reboot SifGetReg(SMFLAG=4), selector `0x7A`, stops at `0x0032F078`, SP `0x01FFED30`, in `FUN_00338108`. The accepted request starts a separate pending IOP lifetime; old service state is invalidated. No reboot completion or ready flag is supplied.

Next: consult the reference architecture and collect fresh disc-backed post-reset IOP boot/service evidence. Determine the ready transition and actual service/import requirements before implementing them; earlier CD snapshots cannot establish the new state.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **87** distinct functions entered / **84** completed across native probes (startup/helpers, not gameplay).
- This pass completes previous partial `FUN_00337F88` and `FUN_00338158`; newly linked `FUN_00338108` reaches the readiness poll. Entry, `FUN_00100388` and `FUN_00338108` remain partial. Previously completed helpers can stop on new inputs.

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
