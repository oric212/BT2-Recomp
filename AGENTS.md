# BT2 project working instructions

Use this repository as the project root. Before continuing development, read docs/PROG.md and docs/project/IMPLEMENTATION_PLAN.md. Consult docs/reference/REFERENCE_ARCHITECTURE.md and its linked sources before architecture or unfamiliar PS2 subsystem work.

## Permanent meaning of "continue"

A plain "continue", including spelling/case variants, requests an aggressive, goal-driven pass. Apply this automatically across sessions; never require the user to repeat the instruction. The pass works through the "Current next step" in docs/project/IMPLEMENTATION_PLAN.md, in order, toward the next functional milestone in docs/PROG.md. It does not chase the latest blocker with a narrow fix.

Each item follows: measure or inspect -> gather evidence -> implement the general behavior (under the architecture rules below) -> regression tests and original comparisons -> Release rerun -> next item. When execution reaches a new boundary, treat it as a hardware or kernel behavior to implement generally at subsystem scale, not as a call site to patch. Gather evidence from local captures/tooling and reference repositories; adapt reference implementations, do not copy them 1:1. Keep unknown function names neutral.

Finishing one item, one fix or one plan step is not a stopping point: advance to the next item in the plan and continue. Stop only when:

- A genuinely new major subsystem needs a separate implementation plan. Write it in the matching docs/plans/ folder; if it is actionable, follow it and continue.
- Required information or access cannot be obtained in the current session.
- The next change would be speculative after consulting available evidence, or would change architecture without the profiling evidence the plan requires.

When stopping, identify the actual condition and evidence.

Build/test meaningful changes. At the end of each pass, update concise docs/PROG.md and detailed docs/progress/ history, review the diff/staged files, commit and push to the existing remote.

Never download or commit proprietary game/BIOS content, retail executables, disc images, extracted assets or proprietary-derived generated code. Keep local content, captures, prepared/generated code and build output ignored. README.md stays at the root; project documentation stays in docs/.


### Work at subsystem scale

Work at subsystem scale. When execution reaches a subsystem boundary, implement the bounded general behavior that subsystem needs (shared by every caller), rather than one handler per function, syscall or call site.

Use docs/reference/REFERENCE_ARCHITECTURE.md and its linked repositories to avoid rediscovering standard PS2 behavior. Verify BT2's reached behavior and adapt patterns; do not copy implementations 1:1. Current priority is the strategy step in docs/project/IMPLEMENTATION_PLAN.md ("Current next step"): profile the startup workload, generalize verified address-gated handlers, then implement GIF DMA chain mode as general behavior. Preserve the evidence and stop conditions above; a newly written actionable plan should guide continued implementation when safe.

## Architecture and performance

BT2-Recomp is a lightweight native Windows x64 port, not a general-purpose PS2 emulator. Details, profiling criteria and verification requirements: docs/project/IMPLEMENTATION_PLAN.md.

- **Static recompilation stays the core.** EE code runs as generated native C++. No EE interpreter, full-system emulation, duplicate subsystem implementations or needless abstraction.
- **Only what BT2 requires.** Add kernel, hardware and device behavior when BT2 reaches it, with evidence; prefer the simplest native implementation that meets the demonstrated requirement.
- **General behavior over workarounds.** Implement verified hardware behavior generally instead of adding handlers gated on guest PCs, stack offsets or call counts. Generalize an existing gate when the same behavior is reached again.
- **Preserve semantics; fail explicitly.** Keep DMA ordering, interrupts, synchronization and memory semantics correct. Unsupported behavior stops at a checked boundary; never fake success.
- **Measure before optimizing.** The software GS renderer and the IOP execution engine are the first investigation targets. Profile Release builds on representative workloads before optimizing or redesigning. GPU-backed rendering or leaner native IOP hot paths need profiling evidence and their own plan.
- **Correctness first.** Validate with the existing tests and original-game comparisons; add a regression test for every generalized behavior.
- **Honest status.** Keep Verified separate from Operational or untested behavior. Never infer gameplay performance from startup progress or a first-frame milestone.
- **No speculative rewrites.** Apply these rules to new work; change architecture only on measured evidence.

## Build efficiency

During investigation, prefer the fastest narrow validation path first: unit tests, offline tools and data checks (for example decoding a captured or dumped buffer in Python), subsystem tests, targeted builds and existing diagnostics. Do not rebuild `bt2-entry-probe` or ALL after every small edit; a change to `ps2_runtime.h`, `gs_command_sink.h` or another header reached by the generated units recompiles all of them. Batch related changes, iterate with narrow checks, and run the expensive full build/probe only when the change set is stable enough for an integration gate or before commit. This changes when expensive gates run, not how much is verified: every pass still completes the full build, test and probe gate before commit.

Every generated unit includes `ps2_runtime.h` (generated IOP modules include `native_runtime.h`). Keep those headers and `gs_command_sink.h` to declarations, state and small hot helpers. Put implementation in the focused `.cpp` files of their folders (`runtime/ee/ps2_runtime_*.cpp`, `runtime/gs/`, `runtime/iop/core/`), where an edit rebuilds one unit in seconds. MSVC targets compile in parallel (`/MP`), and the entry probe precompiles stable standard headers only. Do not use unity builds, and do not merge generated units.

Run selection loops and other long probe runs with the Release configuration (`run_startup_probe.py --config Release`): its output is identical to Debug and it runs about 7× faster. The pre-commit gate still runs Debug.

## Build artifacts after a pass

Keep only intentional build outputs and explicitly required diagnostic artifacts. Delete temporary/intermediate build residue, stale logs, abandoned builds, old comparison dumps, backups, and other disposable pass-generated files. Never delete source inputs, required probe inputs, reference data, or intentional test/diagnostic artifacts without verifying they are unused.

## Code organization

Keep the codebase clean and modular:
- **Focused files:** prefer small files with one clear purpose over large monolithic files, where the functionality separates logically.
- **Clear dependencies:** keep responsibilities separate and dependencies explicit.
- **Check before adding:** before adding substantial code to an existing file, consider whether it belongs in its own focused file or module.
- **No artificial splits:** organize by meaning, readability and maintainability.
- **Preserve boundaries:** keep existing behavior and the generated-code boundaries.

[docs/guides/RUNTIME_LAYOUT.md](docs/guides/RUNTIME_LAYOUT.md) maps the `runtime/` folders (EE, GS, host, IOP core/devices/providers/services/EE bridge, probes, tests), the include convention and where new code goes.

## Documentation organization

Keep `docs/` sorted by purpose:

- `docs/PROG.md`: current progress (read by `tools/docs/sync_progress.py`; keep it at this path).
- `docs/progress/`: the active-pass handoff, monthly history and dashboard data.
- `docs/guides/`: how to build, use the tools, and find your way around the runtime (BUILDING, TOOLS, RUNTIME_LAYOUT).
- `docs/project/`: the implementation plan, progress-tracking rules and legal/licensing.
- `docs/reference/`: reference architecture and reverse-engineering evidence.
- `docs/plans/<subsystem>/`: subsystem implementation plans, grouped as `ee`, `iop`, `graphics`, `sound`, `disc`, `input` and `memory-card`. Create a new subsystem folder only when no existing one fits (for example, `plans/video/` for IPU decoding).

Put every new document in the matching folder, never loose in `docs/`. When a file moves, update every relative link and every `docs/...` path that refers to it (Markdown, code comments with full paths, CMake messages). Check that all links resolve before committing. Keep historical entries unchanged except for those path updates.

## Documentation encoding

Read and write documentation explicitly as UTF-8. Preserve historical entries when appending; do not round-trip them through the Windows default code page. Check changed files for replacement characters and newly introduced mojibake before committing.

## Usage-aware passes

Preserve aggressive subsystem-scale passes while usage is plentiful. Reduce scope for the five-hour allowance only when remaining usage is low; do not use it to justify small passes routinely. Check the actual allowance before making that decision.

## Progress and interrupted-pass tracking

Follow docs/project/PROGRESS_TRACKING.md. Read docs/progress/ACTIVE_PASS.md when resuming unfinished work; record pending changes and remaining verification before an interruption. Keep docs/PROG.md's verified checkpoint distinct from unverified implementation. Export only allowlisted counters from ignored startup reports, synchronize the marked README dashboard with tools/docs/sync_progress.py, and run its --check before progress commits. Detailed completed outcomes belong in monthly history. Never count isolated service coverage as full startup coverage.

## Parallel evidence gathering

Use sub-agents when useful for independent investigation, reference comparison, capture analysis or test design. Keep scheduler, RPC and runtime state changes coordinated by the primary agent. Parallelize evidence gathering and assign clear file ownership for independent tooling/test edits; avoid speculative parallel runtime edits and concurrent native builds/probes.
