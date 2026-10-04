# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The checkpoint probe now completes the first interrupt acknowledgement/wait/acknowledgement sequence and its generated caller, then reaches GIF DMA setup. The event schedule and GS readbacks remain explicit diagnostic checkpoints; native video timing, IOP execution and rendering remain incomplete.

Latest verification: MSVC full checkpoint build/pipeline, seven CTests, 49 capture tests and 42 recomp-tool tests passed. Proprietary content, captures, prepared/generated code and automatic bindings remain ignored.

## Current blocker and next step

A 32-bit GIF DMA CHCR read at `0x1000A000`, PC `0x003100E4`, SP `0x01FFEAF0`, in `FUN_003100D0`. No graphics transfer has been accepted.

Next: implement [the GIF DMA plan](GIF_DMA_BRINGUP_PLAN.md). Capture reached control/status and packet evidence before modeling channel behavior or acknowledging transfers. The interrupt checkpoint preserves clear/poll/event/clear ordering but is not a native video clock.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **101** distinct functions entered / **97** completed across native probes (startup/helpers, not gameplay).
- This pass enters one additional function and completes two previously partial helpers. Entry, `FUN_00100388`, `FUN_00100258` and `FUN_003100D0` remain partial. No rendered frame or complete game boot.

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
