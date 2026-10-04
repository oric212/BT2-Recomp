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

## Repository workflow

Current next step: follow [the GIF DMA plan](GIF_DMA_BRINGUP_PLAN.md) at channel control read in FUN_003100D0. The bounded interrupt ordering experiment and later explicit CSR field checkpoint let generated setup helpers return. Actual video timing, rendering and IOP execution remain unimplemented; retain strict unsupported boundaries.

Before decisions about architecture or unfamiliar PS2 subsystems, read REFERENCE_ARCHITECTURE.md and consult the linked repositories first (BT3 primary, Chronicle where useful). Adapt proven tooling/runtime patterns to BT2 observations; never copy implementations 1:1, game-specific patches/symbols or unsupported success defaults. Keep BT2-specific behavior separate and evidence-based.

Use the scalable loop: provisional function discovery/map -> generation -> compile/link -> execution -> actual blocker -> runtime fix or checked patch -> repeat. Neutral naming and provenance remain required. Existing careful entry reviews validate tooling; do not require exhaustive manual review of every function before experimental local generation. See `REFERENCE_ARCHITECTURE.md` for the inspected BT3-Recomp and Chronicle references.

Project root: `D:\Cs\Cpp\BT2-Recomp`. After each completed pass, verify the result, review the diff and staged files, commit, and push to GitHub. Keep README.md at the root.
