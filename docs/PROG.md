# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The optional EE probe completes **ten native IOP module loads**: SIO2MAN, MCMAN, MCSERV, DBCMAN, SIO2D, DS2U_D, LIBSD, SDRDRV, MODHSYN and MODSESQ2. The four sound-module resident entries now match local reference checkpoints; SDRDRV's RPC worker is parked. Initial controller polls remain verified. No displayed game frame, audio playback or complete boot yet.

Latest verification: MSVC build, **14/14 CTests**, **11/11 IOP tooling tests**, **86/86 capture tests**, four sound-module comparisons and DS2U_D/direct initial poll comparison. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

EE LOADFILE requests **`cdrom0:\IRX\CDVDSTM.IRX;1`**, the disc-streaming driver, without a compiled native definition. Checkpoint: **643 calls / 635 returns**, ten acknowledged residents, **7,856 cumulative IOP instructions / 72 physical entry targets / nine waiting workers**.

Next scope: establish a bounded CDVDSTM/disc-streaming plan from its reached entry, public behavior and local captures, then generate/link/run it. This is a new subsystem boundary; sound registration does not establish SPU2 voice/RAM/DMA behavior or successful sound RPC processing. A Vblank producer, later controller commands, asset loading and presentation remain incomplete. [SIO2 scope](SIO2_BRINGUP_PLAN.md), [Vblank timing](IOP_VBLANK_PLAN.md).

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- Ten EE module-load helper paths now complete; updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered at its initial resident checkpoint**, with one worker parked then (now active at the transfer blocker). MCMAN: 167 provisional ranges / 35 imports; resident result 2 validated against a local reference return and normalized startup data/BSS. MCSERV and DBCMAN resident entries/worker waits are validated; SIO2D resident entry and Vblank wait are validated locally. DS2U_D: 40 provisional ranges / 25 imports; resident return and owned worker start/dormant state are verified. Live controller data/BSS is not equated across Vblank phases. These startup counts do not represent working gameplay functions.
- Added sound maps: LIBSD **57 ranges / 19 imports**, SDRDRV **7 / 52**, MODHSYN **174 / 24**, MODSESQ2 **46 / 9**. These are provisional discovery counts, not restored gameplay functions. Their resident startup paths work; substantive sound processing remains unverified.
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
