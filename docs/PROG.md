# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated recompilation pipeline work locally. Checked startup completes CD setup, SIF handler shutdown, bounded SIF2 stop, the post-stop command-buffer query and the first reset SMFLAG write. Generated code now prepares the real IOP reboot packet. Complete boot, IOP image loading/reset completion, disc access and gameplay remain incomplete.

Latest verification: MSVC build/full checkpoint pipeline, seven CTests, 42 recomp-tool tests and 36 capture-tool tests passed. Proprietary files, captures, generated code and automatic bindings remain ignored. Retail revision is confirmed; function boundaries remain provisional.

## Current blocker and next step

SifSetDma (`0x77`) stops at resume `0x0032F028`, SP `0x01FFEC80`. Descriptor: `[0x003ACC40,0x00019600,104,0x44]`. Command `0x80000003` requests IOP reboot with `rom0:UDNL cdrom0:\IRX\IOPRP300.IMG;1`. The reboot has not been submitted or completed.

Next: consult the reference architecture first, validate the user's local IOP image and collect disc-backed reset/receive evidence. Establish a separate IOP boot/service lifetime; do not reuse the earlier CD snapshot or fabricate readiness.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **86** distinct functions entered / **82** completed across native probes (startup/helpers, not gameplay).
- This pass completes previous partial `FUN_0032EF30` and newly linked `FUN_0032F060`. Entry, `FUN_00100388`, `FUN_00338158` and `FUN_00337F88` remain partial. Previously completed helpers can stop on new inputs.

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
