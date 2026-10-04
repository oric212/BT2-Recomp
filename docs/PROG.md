# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The generated probe consumes both startup GS clears, completes fresh SIF transport initialization, and now returns from post-reset CD initialization and media-mode setup. Validated epoch-owned replies run through the generated EE callback and semaphore cleanup. No displayed frame, native IOP execution or general disc backend is established.

Latest verification: MSVC checkpoint pipeline, nine CTests, 79 capture tests and 43 recomp-tool tests. Local content, captures and generated code remain ignored.

## Current blocker and next step

LOADFILE service bind: syscall `0x77`, PC `0x0032F028`, SP `0x01FFEB50`, requesting `0x80000006` for client `0x003ACC00` through fresh IOP buffer `0x1E640`. Full probe: **400 calls / 391 returns**. Old service bindings remain invalid; no loader reply or successful module load is supplied.

Next: [LOADFILE and IOP module loading](LOADFILE_IOP_MODULE_PLAN.md). Establish the requested loader reply and module path, then plan module ownership, relocation/import resolution and execution from BT2 evidence.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **108** distinct functions entered / **103** completed across native probes (startup/helpers, not gameplay).
- Added `FUN_00333B48` (completed), `FUN_00337C28`, `FUN_00337A00` and `FUN_00337228` (partial at loader bind). Existing CD/media helpers complete again. Counts describe startup helpers, not restored gameplay; entry/main remain partial.
- Two drawing requests / two logical DMA completions / **573,440** sprite pixels processed, including depth writes. No rendered/presented frame or complete game boot.

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

A plain "continue" automatically invokes the permanent pass rule: [continue through multiple blockers](IMPLEMENTATION_PLAN.md#continue-through-multiple-blockers), gathering available evidence before stopping under the documented conditions.

- [October 2026 detailed history](progress/2026-10.md): all previous progress entries, evidence, counts and historical milestone snapshots preserved. Statements about "current" work inside that archive describe their original point in time.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [tool workflow](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

Keep this page limited to current status. Append detailed pass results to `progress/YYYY-MM.md`, linking new monthly logs here. Update the blocker, counts and checklist after verification.
