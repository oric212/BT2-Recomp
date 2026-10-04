# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The generated probe consumes both startup GS clears and now completes the second SIF initialization after the IOP reboot. A separate transport lifetime installs the fresh command buffer and delivers the validated SET_SREG response. No displayed frame, native video clock or native IOP execution is established.

Latest verification: MSVC checkpoint pipeline, nine CTests, 66 capture tests and 43 recomp-tool tests. Local content, captures and generated code remain ignored.

## Current blocker and next step

Fresh RPC bind submission: syscall `0x77`, PC `0x0032F028`, SP `0x01FFEB90`, requesting service `0x80000592` through IOP buffer `0x1E640`. Full probe: **305 calls / 298 returns**. Old service bindings remain invalid; transport initialization does not establish service execution.

Next: [post-reset IOP service ownership and delivery](POST_RESET_IOP_SERVICES_PLAN.md). The fresh live capture contains a candidate service record at `0x43388`, while the old address contains code. Verify the new bind/reply and service lifetime before accepting responses.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **104** distinct functions entered / **102** completed across native probes (startup/helpers, not gameplay).
- This pass adds and completes `FUN_0032F040` and `FUN_00331840`; re-entered SIF initialization now returns. Counts describe startup helpers, not restored gameplay. Entry/main remain partial.
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
