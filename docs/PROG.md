# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The generated probe consumes both startup GS clears and completes fresh SIF, CD/media-mode initialization, LOADFILE binding and its version query. It now submits the first real SIO2MAN module-load request. Validated replies execute the generated EE callback and cleanup; no module-load success is supplied.

Latest verification: MSVC EE checkpoint pipeline, 11 CTests (including new native IOP synthetic programs) and 11 IOP-tool tests. Earlier capture/recomp checks remain documented in history. Local content, captures and generated/relocated code remain ignored.

## Current blocker and next step

Actual module load: syscall `0x77`, PC `0x0032F028`, SP `0x01FFEB50`; LOADFILE RPC 0 requests `cdrom0:\IRX\SIO2MAN.IRX;1` with no module arguments. Full probe: **433 calls / 425 returns**. Module entry/import effects must execute before success can be returned.

The isolated native SIO2MAN entry now **returns result 0 after 151 instructions**. Owned event/semaphore/thread, IRQ registration and DMA setup work; its native thread yields at WaitEventFlag for mask `0x4155`. Normalized initialization effects match the local reference capture. Opaque options, transfer behavior and physical scheduling are not validated.

Next: connect the persistent native IOP instance to the actual pending EE request using [the LOADFILE lifetime bridge plan](IOP_LOADFILE_BRIDGE_PLAN.md), then continue subsequent requests. [SIO2 scope and limits](SIO2_BRINGUP_PLAN.md). No module-success reply has been sent to EE.

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed across native probes (startup/helpers, not gameplay).
- EE counts are unchanged this pass. Module-load helpers, main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered**, one resident module entry returned and one worker retained waiting. These startup counts do not represent working gameplay functions.
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
