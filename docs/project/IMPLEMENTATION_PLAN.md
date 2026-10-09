# Implementation plan

Target: *Dragon Ball Z: Budokai Tenkaichi 2*, North American PS2 release SLUS-21441, running the original game logic natively on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20 and CMake.

**Goal:** a lightweight native Windows x64 port, not a general-purpose PS2 emulator. The game's EE code is statically recompiled to C++; a small runtime supplies only the PS2 kernel, hardware and device behavior BT2 actually uses. The first major gameplay goal is a playable versus battle.

## Architecture rules

### Scope

- **Static recompilation stays the core.** EE code runs as generated native C++. Do not add an EE interpreter, a dynamic recompiler or a full-system emulation layer.
- **Implement what BT2 requires.** Add kernel services, hardware registers and device behavior when BT2 reaches them, with evidence. Do not implement unreached PS2 features.
- **One implementation per subsystem.** Do not keep parallel or duplicate models of the same hardware (for example, two DMA engines or two GS paths). Extend the existing one.
- **No needless abstraction.** Prefer the simplest direct native code that meets the demonstrated requirement. Introduce interfaces or layers only when a second real user exists.

### General behavior over workarounds

- **Model hardware behavior, not call sites.** Prefer a general implementation of verified PS2 behavior (a DMA channel mode, a register, a kernel service) over a handler gated on a guest PC, stack offset or call count. Address-specific gates are acceptable only as a temporary checked boundary with evidence, and should be generalized when the same behavior is reached again.
- **Preserve semantics.** Keep DMA ordering and completion, interrupt delivery order, thread scheduling and synchronization, and memory semantics (including uncached and scratchpad accesses) consistent with the hardware and the original game's observed behavior.
- **Fail explicitly.** Unsupported behavior stops at a checked boundary that names what was reached. Never return a fabricated success value to keep the game running.

### Correctness over shortcuts

- Validate every change with the existing tests and the original-game comparisons (`tools/capture_ee/verify_*.py`, phase stops).
- Add a regression test for each generalized behavior: unit tests for the hardware rule and, where possible, a comparison against a PCSX2 capture of the original game.
- Keep the status vocabulary of [PROG.md](../PROG.md): ✅ Verified, 🟢 Operational, 🔧 Incomplete, ⛔ Blocking, ⏳ Not Reached. Operational or untested behavior is never reported as verified.
- Never infer gameplay performance from startup progress, function counts or a first-frame milestone. Performance claims need a measurement of a representative workload.

## Performance and profiling

### Where to look first

1. **Software GS renderer** (`runtime/gs/`): the largest measured cost. On the 650-frame startup run it took 92% of run time before the last optimizations and about 75% after them (Release).
2. **IOP execution engine** (`runtime/iop/core/`): IOP modules are already translated to C++, but every translated instruction calls `tick()` (step counter, budget check, executing pc). About 10.9 million IOP instructions run in the startup workload; measure what this per-instruction overhead and the IOP time model cost.

### Profiling criteria

- **Release builds only** (`run_startup_probe.py --config Release`); Debug timings are not representative.
- **Representative workloads:** the full startup run (boot to the current boundary, 650 frames) now; a scene with real 3D rendering and a battle once reached. Record the workload, frame count, build and commit with every measurement.
- **Per-subsystem breakdown:** measure EE generated code, GS rasterization, GIF/VIF processing, IOP execution, the scheduler and VBlank delivery, and probe/report overhead separately (sampling profiler plus subsystem timers).
- **Decide from data.** Optimize or redesign a subsystem only when it dominates a representative workload, and record the before/after measurement in the monthly history. Report results as time per frame or share of run time, never as projected gameplay performance.

### Redesigns that need profiling evidence first

- **GPU-backed GS rendering**, with [paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference) as the architecture reference. Consider it only if the optimized CPU rasterizer still dominates representative gameplay frames.
- **Leaner native IOP execution** for hot paths, for example removing per-instruction bookkeeping from verified hot loops. Consider it only if IOP execution is a measured bottleneck.

Neither is started on speculation, and neither replaces working verified code without passing the same tests and comparisons.

## Milestones

| Milestone | Status | Evidence |
| --- | --- | --- |
| Native foundation, local game validation, ELF inspection | ✅ Verified | Retail revision confirmed (`config/retail_revision.json`) |
| PS2Recomp generation and integration | ✅ Verified | Provisional map, local generation, compile/link/execute |
| Boot and startup to the main frame loop | ✅ Verified | Original comparisons; see [PROG.md](../PROG.md) |
| First presented frame; controller and memory card checks | ✅ Verified | 650 frames in the live window |
| Opening movie | ⛔ Blocking | GIF DMA chain mode, then IPU MPEG decoding |
| Title screen, demo fight, menus | ⏳ Not Reached | |
| Character selection and battle loading | ⏳ Not Reached | |
| Playable versus battle | ⏳ Not Reached | First major gameplay goal |
| Audio and a full match | ⏳ Not Reached | |

## Current next step: change of strategy

Until now each pass followed the next blocker and often added a narrowly gated handler for it. Startup is complete and the game is entering scene code (movie, title, gameplay), where the same hardware behavior will be reached many times. The next work therefore shifts from per-blocker workarounds to general, measured implementations:

1. **Profile the current workload.** Measure the 650-frame Release startup run per subsystem (GS, IOP, EE, GIF/VIF, scheduler, probe overhead) and record the baseline in the monthly history.
2. **Audit the address-gated handlers.** List the handlers in `runtime/ee/` and `runtime/iop/` that are gated on a guest PC, stack offset or call count. Mark which describe general hardware behavior already verified at more than one site. Generalize those first, each with a regression test and the existing comparisons kept passing.
3. **Implement GIF DMA chain mode generally.** Implement the current blocker as the general DMA source-chain walker (tag IDs, ordering, QWC/TADR/MADR updates, completion and interrupts), shared with VIF1 chains instead of a separate path, with unit tests per tag type. Then continue toward the movie.
4. **Plan video decoding** in `docs/plans/video/` before implementing IPU MPEG decoding.

Steps 1 and 2 are bounded: they change no game-visible behavior and are not an architecture rewrite. Any redesign they suggest needs its own plan and the profiling evidence above.

## Working method

### Continue through multiple blockers

A plain "continue" (including spelling/case variants) runs a pass through multiple blockers, as AGENTS.md defines: generate/link → run → inspect the blocker → gather evidence → fix → rerun → repeat. If a reached subsystem has an actionable plan, follow it and continue. Stop only when a genuinely new major subsystem needs a plan, when required information or access is unavailable, or when the next change would be speculative.

When more evidence is needed, use local captures, the tooling, [the reference architecture](../reference/REFERENCE_ARCHITECTURE.md) and its linked repositories. Adapt reference behavior to BT2's observed behavior; never copy implementations 1:1, game-specific patches, symbols or unsupported success defaults.

### Work at subsystem scale

When several blockers belong to one subsystem, implement the bounded general behavior that subsystem needs, rather than one handler per blocker. Apply the architecture rules above to every new implementation.

### Evidence and content rules

- Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and generated code in ignored `game/generated/`. Never download or commit proprietary content.
- Record confirmed facts, hypotheses and unknowns separately. Keep unknown function names neutral until evidence supports renaming.
- Use the scalable loop: provisional function discovery/map → generation → compile/link → execution → actual blocker → runtime fix → repeat. Neutral naming and provenance remain required.

### End of a pass

Test the changes, update [PROG.md](../PROG.md) and the monthly history in `docs/progress/`, review the diff and staged files, commit and push. Earlier per-pass handoffs are archived in the [October 2026 history](../progress/2026-10.md).
