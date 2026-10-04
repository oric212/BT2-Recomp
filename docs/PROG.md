# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The generated probe now executes both startup clear packets: two 640 by 448 PSMCT32 color/PSMZ24 depth sprites are consumed in a scoped software sink, and the drawing/synchronization helpers return. It reaches the second SIF initialization after the IOP reboot. DMA/event timing remains diagnostic; no displayed frame, native video clock or IOP execution is established.

Latest verification: MSVC full checkpoint build/pipeline, eight CTests, 58 capture tests and 43 recomp-tool tests passed. Proprietary content, captures, prepared/generated code and automatic bindings remain ignored.

## Current blocker and next step

A DMAC_STAT read at `0x1000E010`, PC `0x003319CC`, SP `0x01FFECA0`, in `FUN_00331888`, reached from the second SIF initialization at `0x001003E0`. Full probe: 265 calls / 261 returns. IOP epoch 1 remains pending; earlier service/channel checkpoints cannot be reused.

Next: [post-reset SIF transport bring-up](POST_RESET_SIF_PLAN.md). Capture the later caller/register state, establish fresh transport ownership, then continue to the actual service blocker.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **102** distinct functions entered / **100** completed across native probes (startup/helpers, not gameplay).
- This pass adds `FUN_00310548`; it, `FUN_003100D0` and `FUN_00100258` now complete. Re-entered SIF routines completed in the earlier lifetime but are partial in this invocation. Entry/main remain partial.
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
