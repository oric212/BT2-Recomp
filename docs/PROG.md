# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The native checkpoint probe now passes the post-reboot readiness bit test, completes two more helpers and reaches a graphics setup boundary. The ready bit is an explicit local reference checkpoint replay; native IOP boot/service execution and complete game startup remain incomplete.

Latest verification: MSVC full checkpoint build/pipeline and seven CTests passed; 42 capture-tool tests passed. Earlier 42 recomp-tool/five IOP-inventory tests remain the last verification for unchanged tools. Proprietary content, captures, generated code and automatic bindings remain ignored.

## Current blocker and next step

Mapped call to `FUN_0030FDC0` from `0x0010026C`, resume `0x00100274`, SP `0x01FFEB00`. Its generated code includes 64-bit GS register access at `0x12001000`; this new subsystem needs [the scoped GS bring-up plan](GS_BRINGUP_PLAN.md).

Next: link the reached GS setup entries and capture the actual register/syscall requirements. Implement only verified behavior. IOP reset/service lifetime remains pending in the native model; its observed ready bit cannot establish native IOP execution.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **89** distinct functions entered / **86** completed across native probes (startup/helpers, not gameplay).
- This pass completes previous partial `FUN_00338108` and newly linked `FUN_0032F0E0`; enters `FUN_00100258`. Entry, `FUN_00100388` and `FUN_00100258` remain partial. The reached GS helper is not yet entered.

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

Permanent pass rule: [continue through multiple blockers](IMPLEMENTATION_PLAN.md#continue-through-multiple-blockers), gathering available evidence before stopping under the documented conditions.

- [October 2026 detailed history](progress/2026-10.md): all previous progress entries, evidence, counts and historical milestone snapshots preserved. Statements about "current" work inside that archive describe their original point in time.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [tool workflow](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

Keep this page limited to current status. Append detailed pass results to `progress/YYYY-MM.md`, linking new monthly logs here. Update the blocker, counts and checklist after verification.
