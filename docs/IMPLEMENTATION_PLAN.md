# Implementation plan

Target: Dragon Ball Z: Budokai Tenkaichi 2, North American PS2 SLUS-21441, running original game logic on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20, and CMake.

The first major gameplay goal is a playable versus battle. Use a custom compatibility runtime for only the PS2 behavior BT2 requires. PS2Recomp is a prospective recompilation dependency, Ghidra the installed analysis tool, and PCSX2 a behavioral reference.

## Ordered steps and completion evidence

1. Native foundation: build and run `bt2-port`; document local content rules. Complete.
2. Local game validation: read the user's executable, check basic format and boot reference, and record size and SHA-256. Confirm the retail revision only against a trusted reference. Complete: local ISO matches official Redump size/MD5/SHA-1, and its embedded ELF matches the local executable. Reference provenance is in `config/retail_revision.json`.
3. ELF inspection: validate program/section table bounds and record entry point and load regions from the supplied file. Complete for local metadata; an empty load record's alignment anomaly remains documented. Keep raw reports in ignored `work/`. Do not infer function purposes.
4. PS2Recomp generation and integration: pinned tools, synthetic validation, Ghidra discovery, automatic provisional EE map preparation, and local generation are working. One generated BT2 syscall wrapper compiles/links and executes through an isolated authored adapter. Extend the runtime from observed compile/execution blockers; proprietary-derived output remains ignored.
5. Runtime entry: generated entry now compiles, links and executes initialization and nested startup. With an explicit kernel snapshot, scans return and startup completes AddIntcHandler registration and completes the Timer 2 mode write and completes EnableIntc with an explicit mask checkpoint and validates Timer 2 readback and creates a dormant thread and reaches ChangeThreadPriority after FlushCache operations `0`/`2`, a bounded kernel patch and explicit captured Timer 3 read replay. Implement only the required compatibility calls from evidence and record each new execution blocker.
6. First rendered frame, title screen, and main menu: incrementally implement the required graphics and platform behavior using observed results and PCSX2 as a behavioral reference.
7. Character selection and battle loading: validate required data access and gameplay transitions.
8. Playable versus battle: reach the first major gameplay goal with working controls.
9. Audio and a full match: verify sound, match completion, and return flow.

## Working rules

Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and proprietary-derived generated code in ignored `game/generated/`. Never download or commit proprietary content. Record confirmed facts, hypotheses, and unknowns separately. Preserve unknown function names until evidence supports renaming. Implement only PS2 behavior required by BT2.

After each verified step, update [the current status](PROG.md) and append detailed evidence to `progress/YYYY-MM.md`. Keep `PROG.md` concise and link each monthly history file there. Keep `README.md` at the repository root and other documentation under `docs/`. Review staged files before committing.

## Repository workflow

Current next step: establish ChangeThreadPriority (`0x29`) return/state behavior for main ID 1, priority 1 at `0x0032EAF8`, then implement the observed request. Bounded CreateThread/GetThreadId work in the authored reference model; dormant ID 2 has not run. The generated helper's early -1 guard path is validated with probe IE clear, not its other branches. Counter progression, scheduling and interrupt delivery remain unimplemented. Preserve exact memory bounds and earlier optional-capture boundaries.

Use the scalable loop: provisional function discovery/map -> generation -> compile/link -> execution -> actual blocker -> runtime fix or checked patch -> repeat. Neutral naming and provenance remain required. Existing careful entry reviews validate tooling; do not require exhaustive manual review of every function before experimental local generation. See `REFERENCE_ARCHITECTURE.md` for the inspected BT3-Recomp and Chronicle references.

Project root: `D:\Cs\Cpp\BT2-Recomp`. After each completed pass, verify the result, review the diff and staged files, commit, and push to GitHub. Keep README.md at the root.
