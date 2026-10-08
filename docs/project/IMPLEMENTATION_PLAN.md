# Implementation plan

Target: Dragon Ball Z: Budokai Tenkaichi 2, North American PS2 SLUS-21441, running original game logic on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20, and CMake.

The first major gameplay goal is a playable versus battle. Use a custom compatibility runtime for only the PS2 behavior BT2 requires. PS2Recomp is a prospective recompilation dependency, Ghidra the installed analysis tool, and PCSX2 a behavioral reference.

## Ordered steps and completion evidence

1. Native foundation: build and run `bt2-port`; document local content rules. Complete.
2. Local game validation: read the user's executable, check basic format and boot reference, and record size and SHA-256. Confirm the retail revision only against a trusted reference. Complete: local ISO matches official Redump size/MD5/SHA-1, and its embedded ELF matches the local executable. Reference provenance is in `config/retail_revision.json`.
3. ELF inspection: validate program/section table bounds and record entry point and load regions from the supplied file. Complete for local metadata; an empty load record's alignment anomaly remains documented. Keep raw reports in ignored `work/`. Do not infer function purposes.
4. PS2Recomp generation and integration: pinned tools, synthetic validation, Ghidra discovery, automatic provisional EE map preparation, and local generation are working. One generated BT2 syscall wrapper compiles/links and executes through an isolated authored adapter. Extend the runtime from observed compile/execution blockers; proprietary-derived output remains ignored.
5. Runtime entry: bounded generated platform initialization returns to entry using validated local checkpoints. The first helper tail chain now returns to `FUN_00100388`; the next generated path replays two explicit DMA checkpoints, registers/enables channel 5, replays explicit SIF queries and models the first CHANGE_SADDR receive effects. Its initialization helper returns; the next path reaches an unsupported RPC bind submission. Resolve only observed mapping, compilation and execution blockers; [current status](../PROG.md) and [history](../progress/2026-10.md) record evidence and limits.
6. First rendered frame, title screen, and main menu: incrementally implement the required graphics and platform behavior using observed results and PCSX2 as a behavioral reference.
7. Character selection and battle loading: validate required data access and gameplay transitions.
8. Playable versus battle: reach the first major gameplay goal with working controls.
9. Audio and a full match: verify sound, match completion, and return flow.

## Working rules

### Continue through multiple blockers

A plain "continue" (including spelling/case variants) invokes this entire rule automatically. The user does not need to repeat "aggressively" or "multiple blockers". Apply it across sessions, including when the preceding message only discusses status. Completing one function or blocker is not sufficient to end a pass.

If a reached subsystem already has an actionable implementation plan, follow that plan and continue; do not repeatedly stop merely because the same subsystem boundary was reached again.

Do not stop after resolving a single blocker. Keep following the execution path through multiple blockers in one pass: generate/link -> run -> inspect the blocker -> gather evidence -> fix -> rerun -> repeat.

When more evidence is needed, gather it from available local captures, tooling, [the reference architecture](../reference/REFERENCE_ARCHITECTURE.md), or its linked repositories, and continue when possible. Adapt reference implementations to BT2's observed behavior; do not copy them 1:1.

Stop only when:

- A genuinely new major subsystem is reached and needs a separate implementation plan.
- Progress requires information or access that cannot be obtained in the current session.
- The next change would be speculative.

At the end of the pass, test the changes, update current progress and detailed history, review the diff and staged files, commit, and push.

Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and proprietary-derived generated code in ignored `game/generated/`. Never download or commit proprietary content. Record confirmed facts, hypotheses, and unknowns separately. Preserve unknown function names until evidence supports renaming. Implement only PS2 behavior required by BT2.

After each verified step, update [the current status](../PROG.md) and append detailed evidence to `progress/YYYY-MM.md`. Keep `PROG.md` concise and link each monthly history file there. Keep `README.md` at the repository root and other documentation under `docs/`. Review staged files before committing.


### Work at subsystem scale

Use a blocker-driven approach, but work at subsystem scale whenever possible. When several blockers belong to the same subsystem, trace and implement the bounded set needed to complete that subsystem instead of stopping after each individual function/syscall.

Use docs/reference/REFERENCE_ARCHITECTURE.md and its linked repositories to avoid rediscovering standard PS2 behavior. Verify BT2's reached behavior and adapt patterns; do not copy implementations 1:1. Current priority is completing startup subsystems far enough to reach game asset loading, real draw submission and presentation. Preserve the evidence and stop conditions above; a newly written actionable plan should guide continued implementation when safe.

## Repository workflow

Current next step: continue from the reached native boundary in PROG.md after the completed first sector read and directory metadata batch. Owned CDVDMAN read state, original IOP completion, logical SIF transport and generated EE callback now load the local directory index. Generated parsing and all five listed file lookups complete; the game returns to its main entry continuation. Follow DISC_COMMAND_PLAN.md and gather evidence for subsequent runtime or archive requests. Pending sound reverse bind, general scheduling, archive-content loading and presentation remain incomplete.

Before decisions about architecture or unfamiliar PS2 subsystems, read REFERENCE_ARCHITECTURE.md and consult the linked repositories first (BT3 primary, Chronicle where useful). Adapt proven tooling/runtime patterns to BT2 observations; never copy implementations 1:1, game-specific patches/symbols or unsupported success defaults. Keep BT2-specific behavior separate and evidence-based.

Use the scalable loop: provisional function discovery/map -> generation -> compile/link -> execution -> actual blocker -> runtime fix or checked patch -> repeat. Neutral naming and provenance remain required. Existing careful entry reviews validate tooling; do not require exhaustive manual review of every function before experimental local generation. See `REFERENCE_ARCHITECTURE.md` for the inspected BT3-Recomp and Chronicle references.

Use the root of your BT2-Recomp checkout for project files and commands. After each completed pass, verify the result, review the diff and staged files, commit, and push to GitHub. Keep README.md at the root.


### Current subsystem handoff

The first owned heap/module-buffer lifecycle completes. Bounded [VIF startup](../plans/graphics/VIF_STARTUP_PLAN.md) and repeated GS setup complete. The bounded [GS texture-state startup](../plans/graphics/GS_TEXTURE_STARTUP_PLAN.md) completes, followed by generated EE pool allocation. The second IOP request completes after verified relocation of conflicting native images. The first [SDRDRV RPC command](../plans/sound/SDRDRV_RPC_PLAN.md) completes. The bounded [EE thread-start scope](../plans/ee/EE_THREAD_START_PLAN.md) and first SOUNDS command complete; the bounded [DBCMAN requests](../plans/input/DBCMAN_RPC_PLAN.md) and reverse transfer now complete; MCSERV initialization now completes; the query now completes bounded card DMA, timed suspension, secondary SIF transport and generated EE completion. Full sound transfers/playback remain separate work. Current measured counts and evidence are in PROG.md and the October history.


The bounded disc scope now completes the directory-sector read, generated parsing and all five listed metadata lookups. CRI bind, its third owned heap block and original stream-slot allocation now complete through generated EE cleanup. The next boundary is [EE cache maintenance](../plans/ee/EE_CACHE_TRANSFER_PLAN.md), before full delivery of the isolated-tested CRI 0x403 command. Follow [CRI protocol evidence](../plans/sound/CRI_RPC_PLAN.md); do not infer streaming, audio playback or presentation.


The reached EE indexed cache scope now completes both original helpers and command 0x403 through generated cleanup. Original command 0x40C reaches higher-priority nested IOP StartThread, with no substituted result. Establish [CRI child-start/preemption ownership](../plans/sound/CRI_THREAD_START_PLAN.md), capture its first wait and retain the active borrowed RPC parent before extending scheduling. Current counts and the separate completed-scope/failing-boundary validation records are in PROG.md.


## CRI child start and AutoDMA handoff (2026-10-06)

The bounded CRI higher-priority child start/first wait is now validated. Current 0x40C execution reaches first AutoDMA enable; follow CRI_AUTODMA_PLAN.md to establish initial buffer, transfer programming and interrupt/callback ownership before acknowledging it. General playback remains incomplete.


## First-channel completion and capacity handoff (2026-10-06)

Original 0x40C and all sixteen 0x422/0x400 pairs now complete with an owned pending initial AutoDMA half. Follow [CRI_SECOND_CHANNEL_PLAN.md](../plans/sound/CRI_SECOND_CHANNEL_PLAN.md): establish later allocation evidence and a consistent IOP ownership layout before admitting the fifth heap request. Its captured pointer overlaps translated FILEIO. Audio consumption/completion remains a separate unimplemented scope.


## Second-channel setup and EE scheduling handoff (2026-10-06)

Both further captured heap allocations and second command-2 / 0x40A setup now complete through the full EE path. Five subsequent 0x408 handlers are independently validated in the original native IOP service test; the full EE checkpoint remains at 37 CRI calls. Follow EE_SOUND_SCHEDULING_PLAN.md for the newly reached worker lifecycle, earlier missing thread identity evidence and retained main/child reverse-RPC ownership. Do not equate isolated service results with full EE execution. Distinct EE/kernel entry counts are now measured automatically; BUILDING.md documents the repeatable startup probe command.


## Four-helper completion and scheduler design gate (2026-10-06)

The capacity/worker pass is verified. Both later heaps, second stream setup, four StartThread/priority transitions and the first/last helper suspensions complete. Full EE startup stops at the main priority request 2 → 24 with 296 / 292 distinct coverage. Follow EE_SOUND_SCHEDULING_PLAN.md to define retained per-thread continuation ownership and reverse-RPC delivery before dispatching the ready child. Original registration/first-wait snapshots exist; live callback timing and bind ownership still need modeling and evidence. The five second-channel pairs remain isolated service proof, not full EE coverage.

## Retained sound diagnostic and queued transport handoff (2026-10-06)

The generated sound registration/first EE wait and actual native IOP reverse bind/first sleep are independently verified in a separate diagnostic scope. Normal startup remains at the pending main priority request. Original first-wait evidence places sound dispatch during the earlier SOUNDS bind, so late dispatch cannot be promoted as the live scheduler. Follow [EE_RPC_SCHEDULING_PLAN.md](../plans/ee/EE_RPC_SCHEDULING_PLAN.md) for persistent invocation ownership and queued EE/IOP completion at the actual blocked-main phase, then follow ready helpers and actual execution blockers.

## Priority admission and interrupt/load handoff (2026-10-06)

Main's priority admission, helper 5/6 critical sections and first waits, CRI stream-buffer copies and VBLANK_START registration are verified in normal startup. Main now polls its first asynchronous request. Follow [EE_INTERRUPT_LOAD_PLAN.md](../plans/ee/EE_INTERRUPT_LOAD_PLAN.md) for owned interrupt delivery, parked-thread wakes and request progress.
