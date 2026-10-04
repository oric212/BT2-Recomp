# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The checkpoint probe now completes initial GS reset/mask/CRT setup and several generated RAM setup helpers. It reaches an interrupt acknowledgement/event-wait boundary. GS readbacks and syscall returns remain explicit local reference checkpoints; native video timing, IOP execution and rendering remain incomplete.

Latest verification: MSVC full checkpoint build/pipeline, seven CTests, 46 capture tests and 42 recomp-tool tests passed. Proprietary content, captures, prepared/generated code and automatic bindings remain ignored.

## Current blocker and next step

A 32-bit write of `4` to INTC_STAT `0x1000F000` at `0x0032F10C`, SP `0x01FFEAD0`, in `FUN_0032F0F0`. The routine subsequently waits for bit `4` to become set again, then acknowledges it again. Separate disc-backed captures reach the completed wait and later CSR field read.

Next: implement [the interrupt/video-timing plan](GS_INTERRUPT_TIMING_PLAN.md). Preserve event acknowledgement and ordering; returning the written value would incorrectly bypass synchronization. Full DMA, renderer and IOP service execution remain unsupported.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **100** distinct functions entered / **95** completed across native probes (startup/helpers, not gameplay).
- This pass enters 11 functions and completes nine. Entry, `FUN_00100388`, `FUN_00100258`, `FUN_003104B0` and `FUN_0032F0F0` remain partial. No rendered frame or complete game boot.

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
