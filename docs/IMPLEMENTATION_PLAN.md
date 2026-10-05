# Implementation plan

Target: Dragon Ball Z: Budokai Tenkaichi 2, North American PS2 SLUS-21441, running original game logic on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20, and CMake.

The first major gameplay goal is a playable versus battle. Use a custom compatibility runtime for only the PS2 behavior BT2 requires. PS2Recomp is a prospective recompilation dependency, Ghidra the installed analysis tool, and PCSX2 a behavioral reference.

## Ordered steps and completion evidence

1. Native foundation: build and run `bt2-port`; document local content rules. Complete.
2. Local game validation: read the user's executable, check basic format and boot reference, and record size and SHA-256. Confirm the retail revision only against a trusted reference. Complete: local ISO matches official Redump size/MD5/SHA-1, and its embedded ELF matches the local executable. Reference provenance is in `config/retail_revision.json`.
3. ELF inspection: validate program/section table bounds and record entry point and load regions from the supplied file. Complete for local metadata; an empty load record's alignment anomaly remains documented. Keep raw reports in ignored `work/`. Do not infer function purposes.
4. PS2Recomp generation and integration: pinned tools, synthetic validation, Ghidra discovery, automatic provisional EE map preparation, and local generation are working. One generated BT2 syscall wrapper compiles/links and executes through an isolated authored adapter. Extend the runtime from observed compile/execution blockers; proprietary-derived output remains ignored.
5. Runtime entry: bounded generated platform initialization returns to entry using validated local checkpoints. The first helper tail chain now returns to `FUN_00100388`; the next generated path replays two explicit DMA checkpoints, registers/enables channel 5, replays explicit SIF queries and models the first CHANGE_SADDR receive effects. Its initialization helper returns; the next path reaches an unsupported RPC bind submission. Resolve only observed mapping, compilation and execution blockers; [current status](PROG.md) and [history](progress/2026-10.md) record evidence and limits.
6. First rendered frame, title screen, and main menu: incrementally implement the required graphics and platform behavior using observed results and PCSX2 as a behavioral reference.
7. Character selection and battle loading: validate required data access and gameplay transitions.
8. Playable versus battle: reach the first major gameplay goal with working controls.
9. Audio and a full match: verify sound, match completion, and return flow.

## Working rules

### Continue through multiple blockers

A plain "continue" (including spelling/case variants) invokes this entire rule automatically. The user does not need to repeat "aggressively" or "multiple blockers". Apply it across sessions, including when the preceding message only discusses status. Completing one function or blocker is not sufficient to end a pass.

If a reached subsystem already has an actionable implementation plan, follow that plan and continue; do not repeatedly stop merely because the same subsystem boundary was reached again.

Do not stop after resolving a single blocker. Keep following the execution path through multiple blockers in one pass: generate/link -> run -> inspect the blocker -> gather evidence -> fix -> rerun -> repeat.

When more evidence is needed, gather it from available local captures, tooling, [the reference architecture](REFERENCE_ARCHITECTURE.md), or its linked repositories, and continue when possible. Adapt reference implementations to BT2's observed behavior; do not copy them 1:1.

Stop only when:

- A genuinely new major subsystem is reached and needs a separate implementation plan.
- Progress requires information or access that cannot be obtained in the current session.
- The next change would be speculative.

At the end of the pass, test the changes, update current progress and detailed history, review the diff and staged files, commit, and push.

Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and proprietary-derived generated code in ignored `game/generated/`. Never download or commit proprietary content. Record confirmed facts, hypotheses, and unknowns separately. Preserve unknown function names until evidence supports renaming. Implement only PS2 behavior required by BT2.

After each verified step, update [the current status](PROG.md) and append detailed evidence to `progress/YYYY-MM.md`. Keep `PROG.md` concise and link each monthly history file there. Keep `README.md` at the repository root and other documentation under `docs/`. Review staged files before committing.


### Work at subsystem scale

Use a blocker-driven approach, but work at subsystem scale whenever possible. When several blockers belong to the same subsystem, trace and implement the bounded set needed to complete that subsystem instead of stopping after each individual function/syscall.

Use docs/REFERENCE_ARCHITECTURE.md and its linked repositories to avoid rediscovering standard PS2 behavior. Verify BT2's reached behavior and adapt patterns; do not copy implementations 1:1. Current priority is completing startup subsystems far enough to reach game asset loading, real draw submission and presentation. Preserve the evidence and stop conditions above; a newly written actionable plan should guide continued implementation when safe.

## Repository workflow

Current next step: validate and implement the bounded [DBCMAN request group](DBCMAN_RPC_PLAN.md). The [EE ready-state transition](EE_THREAD_START_PLAN.md) and first SOUNDS command complete. Bounded [SDRDRV RPC delivery](SDRDRV_RPC_PLAN.md) completes its first command. The [second IOP allocation and memory layout](IOP_HEAP_CAPACITY_PLAN.md) is verified. Initial VIF, repeated GS setup, a third GIF configuration packet and generated EE pool allocations are verified. Fourteen module-load RPCs complete with thirteen residents. Arbitrary heap allocation, sound playback, disc sectors, assets and presentation remain incomplete. Follow [disc-streaming scope](CDVD_STREAMING_PLAN.md) when sector requests are reached. Controller workers remain parked on owned Vblank events without a timing producer.

Before decisions about architecture or unfamiliar PS2 subsystems, read REFERENCE_ARCHITECTURE.md and consult the linked repositories first (BT3 primary, Chronicle where useful). Adapt proven tooling/runtime patterns to BT2 observations; never copy implementations 1:1, game-specific patches/symbols or unsupported success defaults. Keep BT2-specific behavior separate and evidence-based.

Use the scalable loop: provisional function discovery/map -> generation -> compile/link -> execution -> actual blocker -> runtime fix or checked patch -> repeat. Neutral naming and provenance remain required. Existing careful entry reviews validate tooling; do not require exhaustive manual review of every function before experimental local generation. See `REFERENCE_ARCHITECTURE.md` for the inspected BT3-Recomp and Chronicle references.

Project root: `D:\Cs\Cpp\BT2-Recomp`. After each completed pass, verify the result, review the diff and staged files, commit, and push to GitHub. Keep README.md at the root.


### Current subsystem handoff

The first owned heap/module-buffer lifecycle completes. Bounded [VIF startup](VIF_STARTUP_PLAN.md) and repeated GS setup complete. The bounded [GS texture-state startup](GS_TEXTURE_STARTUP_PLAN.md) completes, followed by generated EE pool allocation. The second IOP request completes after verified relocation of conflicting native images. The first [SDRDRV RPC command](SDRDRV_RPC_PLAN.md) completes. The bounded [EE thread-start scope](EE_THREAD_START_PLAN.md) and first SOUNDS command complete; follow [DBCMAN requests](DBCMAN_RPC_PLAN.md) at the new protocol boundary. Full sound transfers/playback remain separate work. Current measured counts and evidence are in PROG.md and the October history.
