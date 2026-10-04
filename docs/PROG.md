# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The checkpoint probe now stages the first GIF DMA request: idle read, count and source address are checked, and the generated packet matches reference semantics. It stops before starting the transfer because the packet draws a sprite. GS/event behavior still uses diagnostic checkpoints; native rendering, video timing and IOP execution remain incomplete.

Latest verification: MSVC full checkpoint build/pipeline, seven CTests, 55 capture tests and 42 recomp-tool tests passed. Proprietary content, captures, prepared/generated code and automatic bindings remain ignored.

## Current blocker and next step

A 32-bit GIF DMA CHCR start write `0x101` at `0x1000A000`, PC `0x003101A8`, SP `0x01FFEAF0`, in `FUN_003100D0`. Source `0x01FFEB50`, QWC 15, includes 14 A+D writes and two XYZ2 kicks. No transfer or drawing effect has been accepted.

Next: implement [the first drawing/renderer plan](GS_RENDERER_BRINGUP_PLAN.md). Consume supported commands before acknowledging DMA or reporting a frame. Three control words differ only in documented unused bits; meaningful packet fields match and guest RAM stays intact.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **101** distinct functions entered / **97** completed across native probes (startup/helpers, not gameplay).
- Function counts are unchanged this pass; execution advances through idle/count/address staging and packet validation. Entry, `FUN_00100388`, `FUN_00100258` and `FUN_003100D0` remain partial. No rendered frame or complete game boot.

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
