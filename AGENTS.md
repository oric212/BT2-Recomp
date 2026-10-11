# BT2 project working instructions

Use this repository as the project root. Before work, read docs/PROG.md and docs/project/IMPLEMENTATION_PLAN.md; consult docs/reference/REFERENCE_ARCHITECTURE.md and its linked sources before architecture or unfamiliar PS2 subsystem work.

## Goal and architecture

BT2-Recomp is a native Windows x64 port with a minimal compatibility layer, not a PS2 emulator. Details and verification requirements are in the implementation plan.

- **Recompiled game code is the base.** EE code runs as generated native C++. Replace individual functions with hand-written C++ only where it pays off. No full decompilation, EE interpreter or full-system emulation.
- **Native services over emulation.** Replace PS2 services with host code at the highest boundary BT2's evidence supports: SIF RPC services (disc, files, sound, controller, memory card, CRI) instead of emulated IOP modules and hardware. Emulate only what the game drives directly (EE DMA to the GS, VIF packets, the EE kernel).
- **GPU rendering through paraLLEl-GS**, fed with the game's GIF/VIF output. The CPU renderer stays as the fallback for systems without a suitable Vulkan GPU (for example some integrated graphics) and as the test reference.
- **Smallest layer.** Implement only what BT2 reaches, in the simplest native form. One implementation per behavior; generalize verified special cases instead of adding handlers gated on guest PCs, stack offsets or call counts; remove code a replacement makes obsolete.
- **Fail explicitly.** Unsupported requests stop at a checked boundary; never fake success.
- **Correctness first.** Game-visible results (EE memory, files, replies, frames) must match the original. Validate with tests and PCSX2 comparisons, and add a test for each service request type and generalized behavior.
- **Honest status.** Keep Verified separate from Operational or untested. Never infer gameplay performance from startup progress; measure Release builds on representative workloads.
- **References:** adapt behavior, never copy implementations 1:1, and never copy decompiled game code (for example Tenkaichi3Decomp's `src/`). Keep unknown function names neutral.

## Meaning of "continue"

A plain "continue" (any spelling or case) runs an aggressive, goal-driven pass, automatically and across sessions. Work through "Current next step" in the implementation plan, in order, toward the next functional milestone: inspect -> gather evidence (captures, tooling, references) -> implement the native replacement or general behavior -> tests and original comparisons -> Release rerun -> next item. A new execution boundary is input to the plan: decide whether it belongs behind a native service or needs general EE-side behavior, and implement it at subsystem scale. Finishing one item or fix is not a stopping point. Stop only when:

- a new major subsystem needs its own plan (write it in docs/plans/; if actionable, follow it and continue);
- required information or access is unavailable in this session;
- the next change would be speculative after consulting the evidence.

When stopping, state the condition and evidence. At the end of each pass, update docs/PROG.md and the monthly history, review the diff/staged files, commit and push.

Reduce pass scope for the five-hour usage allowance only when the actual remaining usage is low.

## Content rules

Never download or commit proprietary game/BIOS content, retail executables, disc images, extracted assets or proprietary-derived generated code. Keep local content, captures, prepared/generated code and build output ignored. README.md stays at the root; documentation stays in docs/.

## Builds

- **Narrow checks first:** unit tests, offline tools and data checks, subsystem tests, targeted builds. Batch related changes; run the full build/probe only for an integration gate or before commit. Every pass still completes the full build, test and probe gate before commit.
- **Headers reached by generated code** (`ps2_runtime.h`, `gs_command_sink.h`) hold declarations, state and small hot helpers only; an edit recompiles every generated unit. Put implementation in the focused `.cpp` files of `runtime/ee/`, `runtime/gs/`, `runtime/services/`. Keep `/MP` and the standard-header PCH; no unity builds; never merge generated units.
- **Long probe runs use Release** (`run_startup_probe.py --config Release`); the pre-commit gate runs Debug.
- **After a pass,** keep only intentional build outputs and required diagnostic artifacts; delete temporary residue, stale logs, abandoned builds, old dumps and backups. Never delete source inputs, probe inputs, reference data or test artifacts without verifying they are unused.

## Code organization

Small focused files with one clear purpose; explicit dependencies; check whether new code belongs in its own file before growing an existing one; no artificial splits; preserve existing behavior and generated-code boundaries. [docs/guides/RUNTIME_LAYOUT.md](docs/guides/RUNTIME_LAYOUT.md) maps the `runtime/` folders and where new code goes.

## Documentation

- **Layout:** `docs/PROG.md` (current progress; read by `tools/docs/sync_progress.py`, keep the path), `docs/progress/` (active handoff, monthly history, dashboard data), `docs/guides/` (BUILDING, TOOLS, RUNTIME_LAYOUT), `docs/project/` (implementation plan, progress tracking, legal), `docs/reference/` (reference architecture, reverse-engineering evidence), `docs/plans/<subsystem>/` (`ee`, `iop`, `graphics`, `sound`, `disc`, `input`, `memory-card`; add a folder such as `video` only when none fits). Never leave a document loose in `docs/`.
- **Links:** when a file moves, update every relative link and `docs/...` path to it (Markdown, code comments, CMake messages) and check that all links resolve. Historical entries change only in such paths.
- **Encoding:** read and write docs as UTF-8; check changed files for replacement characters and mojibake before committing.
- **Progress tracking:** follow docs/project/PROGRESS_TRACKING.md. Read docs/progress/ACTIVE_PASS.md when resuming, and record pending changes and remaining verification before an interruption. Keep the verified checkpoint distinct from unverified work; export only allowlisted counters; run `tools/docs/sync_progress.py` and its `--check` before progress commits. Never count isolated service coverage as full startup coverage.

## Parallel work

Use sub-agents for independent investigation, reference comparison, capture analysis or test design, with clear file ownership. The primary agent coordinates scheduler, RPC and runtime state changes; avoid speculative parallel runtime edits and concurrent native builds or probes.
