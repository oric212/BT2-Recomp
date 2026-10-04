# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The optional EE probe completes **five native IOP module loads**: SIO2MAN, MCMAN, MCSERV, DBCMAN and SIO2D. Driver init and RPC worker startup execute as compiled native code. SIO2D parks on an owned Vblank event. DS2U_D now wakes the actual SIO2MAN worker, which reaches its first device transfer; six other workers remain waiting. Two startup GS clears execute; no displayed game frame or complete boot yet.

Latest verification: full MSVC checkpoint pipeline, **14/14 CTests**, **11/11 IOP tooling tests**, **83/83 capture tests** and normalized local startup comparisons for all five resident modules. Proprietary content, captures and generated code remain ignored.

## Current blocker and next step

DS2U_D calls registered native SIO2D/SIO2MAN exports, acquires an available semaphore, copies owned data and wakes SIO2MAN through real event state. The worker stops at **0x10148**, writing byte **1** to the SIO2 transmit FIFO **0x1F808260**. No device response or transfer completion is supplied. EE checkpoint: **538 calls / 530 returns**, five acknowledged residents, **4,168 cumulative IOP instructions / 44 physical entry targets**. DS2U_D remains pending.

Next scope: establish the observed SIO2 transfer packet, device configuration, response and completion ordering before implementing the controller/device protocol. [SIO2 limits and next scope](SIO2_BRINGUP_PLAN.md), [Vblank timing](IOP_VBLANK_PLAN.md). Vblank timing/resumption and a displayed frame remain incomplete.

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- Five EE module-load helper paths now complete; updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered at its initial resident checkpoint**, with one worker parked then (now active at the transfer blocker). MCMAN: 167 provisional ranges / 35 imports; resident result 2 validated against a local reference return and normalized startup data/BSS. MCSERV and DBCMAN resident entries/worker waits are validated; SIO2D resident entry and Vblank wait are validated locally. DS2U_D: 40 provisional ranges / 25 imports; entry remains partial. These startup counts do not represent working gameplay functions.
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
