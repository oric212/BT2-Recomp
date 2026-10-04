# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The optional generated EE probe now loads compiled SIO2MAN through a persistent native IOP service. Its real entry returns, the generated EE RPC_END callback/cleanup completes, and startup requests MCMAN. MCMAN native entry executes until its first IOMAN driver call. No complete boot or displayed frame yet.

Latest verification: full MSVC EE checkpoint pipeline, **13/13 CTests**, native SIO2MAN startup comparison and IOP structural tests. Proprietary content, captures and generated code remain ignored.

## Current blocker and next step

MCMAN reaches **ioman:21 (DelDrv)** at native PC **0x31BE4**. EE remains at syscall 0x77 / PC 0x0032F028, pending LOADFILE RPC 0 for `cdrom0:\IRX\MCMAN.IRX;1`. Checkpoint: **454 calls / 446 returns**, one acknowledged native resident module, **1,338 cumulative IOP instructions / 12 actual entry targets**. Failed MCMAN is not registered or acknowledged.

Next: [IOMAN driver lifecycle plan](IOP_IOMAN_DRIVER_PLAN.md). Implement owned driver registration/removal with real native init/deinit callbacks, then rerun and follow reached blockers. [EE bridge](IOP_LOADFILE_BRIDGE_PLAN.md) and [SIO2 limits](SIO2_BRINGUP_PLAN.md).

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- This pass completes the first EE module-load helper path; updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered**, one resident module entry returned and one worker retained waiting. MCMAN adds 167 provisional ranges / 35 imports and six actual entry targets so far; its entry remains partial. These startup counts do not represent working gameplay functions.
- Two drawing requests / two logical DMA completions / **573,440** sprite pixels processed, including depth writes. No displayed frame or complete game boot.

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
