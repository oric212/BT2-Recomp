# BT2 project working instructions

Use this repository as the project root. Before continuing development, read docs/PROG.md and docs/IMPLEMENTATION_PLAN.md. Consult docs/REFERENCE_ARCHITECTURE.md and its linked sources before architecture or unfamiliar PS2 subsystem work.

## Permanent meaning of "continue"

A plain "continue", including spelling/case variants, requests an aggressive pass through multiple execution blockers. Apply this automatically across sessions; never require the user to repeat the instruction. Resolving one function or blocker is not a stopping point.

Follow: generate/link -> run -> inspect -> gather evidence -> fix -> rerun -> repeat. Gather available evidence from local captures/tooling and reference repositories, then continue whenever a safe evidence-based change is possible. Adapt reference implementations; do not copy them 1:1. Keep unknown function names neutral.

Stop only when:

- A genuinely new major subsystem requires a separate implementation plan. If an actionable plan already exists, follow it and continue.
- Required information or access cannot be obtained in the current session.
- The next change would be speculative after consulting available evidence.

When stopping, identify the actual condition and evidence. Do not stop merely because one fix succeeded or a convenient number of functions was completed.

Build/test meaningful changes. At the end of each pass, update concise docs/PROG.md and detailed docs/progress/ history, review the diff/staged files, commit and push to the existing remote.

Never download or commit proprietary game/BIOS content, retail executables, disc images, extracted assets or proprietary-derived generated code. Keep local content, captures, prepared/generated code and build output ignored. README.md stays at the root; project documentation stays in docs/.


### Work at subsystem scale

Use a blocker-driven approach, but work at subsystem scale whenever possible. When several blockers belong to the same subsystem, trace and implement the bounded set needed to complete that subsystem instead of stopping after each individual function/syscall.

Use docs/REFERENCE_ARCHITECTURE.md and its linked repositories to avoid rediscovering standard PS2 behavior. Verify BT2's reached behavior and adapt patterns; do not copy implementations 1:1. Current priority is completing startup subsystems far enough to reach game asset loading, real draw submission and presentation. Preserve the evidence and stop conditions above; a newly written actionable plan should guide continued implementation when safe.

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

[docs/RUNTIME_LAYOUT.md](docs/RUNTIME_LAYOUT.md) maps the `runtime/` folders (EE, GS, host, IOP core/devices/providers/services/EE bridge, probes, tests), the include convention and where new code goes.

## Documentation encoding

Read and write documentation explicitly as UTF-8. Preserve historical entries when appending; do not round-trip them through the Windows default code page. Check changed files for replacement characters and newly introduced mojibake before committing.

## Usage-aware passes

Preserve aggressive subsystem-scale passes while usage is plentiful. Reduce scope for the five-hour allowance only when remaining usage is low; do not use it to justify small passes routinely. Check the actual allowance before making that decision.

## Progress and interrupted-pass tracking

Follow docs/PROGRESS_TRACKING.md. Read docs/progress/ACTIVE_PASS.md when resuming unfinished work; record pending changes and remaining verification before an interruption. Keep docs/PROG.md's verified checkpoint distinct from unverified implementation. Export only allowlisted counters from ignored startup reports, synchronize the marked README dashboard with tools/docs/sync_progress.py, and run its --check before progress commits. Detailed completed outcomes belong in monthly history. Never count isolated service coverage as full startup coverage.

## Parallel evidence gathering

Use sub-agents when useful for independent investigation, reference comparison, capture analysis or test design. Keep scheduler, RPC and runtime state changes coordinated by the primary agent. Parallelize evidence gathering and assign clear file ownership for independent tooling/test edits; avoid speculative parallel runtime edits and concurrent native builds/probes.
