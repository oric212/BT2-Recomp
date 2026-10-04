# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The generated probe consumes both startup GS clears and completes fresh SIF, CD/media-mode initialization, LOADFILE binding and its version query. It now submits the first real SIO2MAN module-load request. Validated replies execute the generated EE callback and cleanup; no module-load success is supplied.

Latest verification: MSVC EE checkpoint pipeline, 11 CTests (including new native IOP synthetic programs) and 11 IOP-tool tests. Earlier capture/recomp checks remain documented in history. Local content, captures and generated/relocated code remain ignored.

## Current blocker and next step

Actual module load: syscall `0x77`, PC `0x0032F028`, SP `0x01FFEB50`; LOADFILE RPC 0 requests `cdrom0:\IRX\SIO2MAN.IRX;1` with no module arguments. Full probe: **433 calls / 425 returns**. Module entry/import effects must execute before success can be returned.

An isolated authored MIPS-I backend now compiles and executes the local SIO2MAN entry, with owned IOP RAM and checked import dispatch. Library registration works. It stops after 44 native instructions at PC `0x00010008`: a 32-bit write of `0x3BC` to SIO2 control `0x1F808268`. This new hardware/IRQ scope needs a separate evidence-based plan before module completion. See [native IOP execution](IOP_NATIVE_EXECUTION_PLAN.md). Native scheduling and EE loader connection remain pending.

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed across native probes (startup/helpers, not gameplay).
- EE counts are unchanged this pass. Module-load helpers, main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **three native IOP mapped ranges entered**, one owned library registered. Module entry has not completed; these are partial execution counts, not working gameplay functions.
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
