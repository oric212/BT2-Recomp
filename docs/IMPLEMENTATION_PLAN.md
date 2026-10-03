# Implementation plan

Target: Dragon Ball Z: Budokai Tenkaichi 2, North American PS2 SLUS-21441, running original game logic on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20, and CMake.

The first major gameplay goal is a playable versus battle. Use a custom compatibility runtime for only the PS2 behavior BT2 requires. PS2Recomp is a prospective recompilation dependency, Ghidra the installed analysis tool, and PCSX2 a behavioral reference.

## Ordered steps and completion evidence

1. Native foundation: build and run `bt2-port`; document local content rules. Complete.
2. Local game validation: read the user's executable, check basic format and boot reference, and record size and SHA-256. Confirm the retail revision only against a trusted reference. Preliminary checks implemented; revision confirmation remains open.
3. ELF inspection: validate program/section table bounds and record entry point and load regions from the supplied file. Complete for local metadata; an empty load record's alignment anomaly remains documented. Keep raw reports in ignored `work/`. Do not infer function purposes.
4. PS2Recomp generation and integration: pinned tools, synthetic validation, Ghidra discovery, automatic provisional EE map preparation, and local generation are working. One generated BT2 syscall wrapper compiles/links and executes through an isolated authored adapter. Extend the runtime from observed compile/execution blockers; proprietary-derived output remains ignored.
5. Runtime entry: generated entry now compiles, links and executes CPU initialization and startup RAM clearing, then stops at unsupported SetupThread (`0x3C`). Implement only the required compatibility calls from evidence and record each new execution blocker.
6. First rendered frame, title screen, and main menu: incrementally implement the required graphics and platform behavior using observed results and PCSX2 as a behavioral reference.
7. Character selection and battle loading: validate required data access and gameplay transitions.
8. Playable versus battle: reach the first major gameplay goal with working controls.
9. Audio and a full match: verify sound, match completion, and return flow.

## Working rules

Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and proprietary-derived generated code in ignored `game/generated/`. Never download or commit proprietary content. Record confirmed facts, hypotheses, and unknowns separately. Preserve unknown function names until evidence supports renaming. Implement only PS2 behavior required by BT2.

Update `PROG.md`, the single progress log, after each verified step. Keep `README.md` at the repository root and other documentation under `docs/`. Review staged files before committing.

## Repository workflow

Current next step: integrate reached generated `FUN_003383C8` and execute to its actual blocker. Its observed arguments are `0x80000000`, `0x80080000`, `0x00338388`; determine required memory/privileged behavior from emitted code and pinned references. SetSyscall registrations are recorded, but custom handler invocation remains unsupported.

Use the scalable loop: provisional function discovery/map -> generation -> compile/link -> execution -> actual blocker -> runtime fix or checked patch -> repeat. Neutral naming and provenance remain required. Existing careful entry reviews validate tooling; do not require exhaustive manual review of every function before experimental local generation. See `REFERENCE_ARCHITECTURE.md` for the inspected BT3-Recomp and Chronicle references.

Project root: `D:\Cs\Cpp\BT2-Recomp`. After each completed pass, verify the result, review the diff and staged files, commit, and push to GitHub. Keep README.md at the root.
