# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The optional EE probe completes **six native IOP module loads**: SIO2MAN, MCMAN, MCSERV, DBCMAN, SIO2D and DS2U_D. Both initial controller polls execute through the native SIO2 interrupt/receive path. Port 0 uses the locally verified digital/no-buttons startup profile; port 1 is disconnected. Eight workers remain waiting, including DS2U_D's connected-port worker on Vblank-start. No displayed game frame or complete boot yet.

Latest verification: MSVC build, **14/14 CTests**, **11/11 IOP tooling tests**, **85/85 capture tests**, scoped DS2U_D resident/worker comparison and direct local captures of both first poll replies/status. Proprietary content, captures and generated code remain ignored.

## Current blocker and next step

EE LOADFILE requests **`cdrom0:\IRX\LIBSD.IRX;1`**, which has no compiled native definition yet. Checkpoint: **559 calls / 551 returns**, six acknowledged residents, **7,491 cumulative IOP instructions / 64 physical entry targets**. The earlier FIFO blocker is resolved for the two observed startup polls; later controller/configuration packets are unsupported.

Next scope: inspect/generate LIBSD and establish a bounded SPU2/sound initialization plan from reached code, public SDK/PCSX2 behavior and local reference evidence. Do not substitute a successful load or implement full audio before its actual blockers. [SIO2 scope](SIO2_BRINGUP_PLAN.md), [Vblank timing](IOP_VBLANK_PLAN.md). A Vblank producer, later controller commands, asset loading and presentation remain incomplete.

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- Six EE module-load helper paths now complete; updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered at its initial resident checkpoint**, with one worker parked then (now active at the transfer blocker). MCMAN: 167 provisional ranges / 35 imports; resident result 2 validated against a local reference return and normalized startup data/BSS. MCSERV and DBCMAN resident entries/worker waits are validated; SIO2D resident entry and Vblank wait are validated locally. DS2U_D: 40 provisional ranges / 25 imports; resident return and owned worker start/dormant state are verified. Live controller data/BSS is not equated across Vblank phases. These startup counts do not represent working gameplay functions.
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
