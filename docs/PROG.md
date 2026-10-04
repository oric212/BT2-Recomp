# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated recompilation pipeline work locally. Checked startup now completes the CD-init and CD synchronous-command binds, replays the observed mode-2 media-mode reply, returns to main, and shuts down the SIF handler/mask before entering the next startup path. This is bounded checkpoint-supported compatibility; complete boot, IOP reset, disc loading and gameplay remain incomplete.

Latest verification: MSVC build/full checkpoint pipeline, seven CTests, 42 recomp-tool tests and 30 capture-tool tests passed. Proprietary files, captures, generated code and automatic binding selections stay ignored. Retail revision is confirmed; function boundaries remain provisional.

## Current blocker and next step

Generated `FUN_0032EF30` stops at SifStopDma (`0x6B`), resume `0x0032EF38`, SP `0x01FFEC80`, a0=1, inside the next path through `FUN_00338158` / `FUN_00337F88`. SIF DMA channel 5 is disabled and its sole dormant handler removed; no IOP reset has been performed.

Next: establish the reached DMA-stop/reset lifecycle from local kernel/PCSX2 evidence and public ABI, then continue. The captured standalone media-mode reply is 0; it does not prove media readiness or disc access.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **85** distinct functions entered / **80** completed across native probes (startup/helpers, not gameplay).
- This pass completes eight additional functions, including the previous partial CD helpers. Entry, `FUN_00100388`, `FUN_00338158`, `FUN_00337F88` and `FUN_0032EF30` remain partial. Previously completed helpers can stop on new inputs.

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
