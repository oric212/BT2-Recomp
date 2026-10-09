# Resume handoff

Updated: 2026-10-09. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames). The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Next implementation

Follow "Current next step" in [the implementation plan](../project/IMPLEMENTATION_PLAN.md#current-next-step-change-of-strategy), in this order. Steps 1 and 2 change no game-visible behavior.

### 1. Release profiling baseline

- **Reuse what exists.** The probe report already counts the workload: GS pixels, sprites and triangles, VIF1 transfers, IOP instructions (`native_iop_steps`), EE calls and VBlanks. The earlier GS measurement (92% of the run before optimization, about 75% after) used a temporary in-process sampler and a RelWithDebInfo build. That sampler was a scratch script and is not in the repository.
- **Add only what is missing:** per-subsystem wall time for GS rasterization, GIF/VIF processing, IOP execution, scheduler/VBlank delivery and report writing, behind an opt-in switch. Keep it out of `ps2_runtime.h`, and leave the report counters unchanged.
- **Record a verified baseline** in the monthly history: workload (default startup run, 650 frames), build (Release; RelWithDebInfo only for symbols), commit, total time and each subsystem's share. Repeat it to confirm it is stable.
- **No optimization in this step.** Decide later from the baseline, under the plan's profiling criteria.

### 2. Address-gated handler audit

- **List** the handlers in `runtime/ee/` and `runtime/iop/` gated on a guest PC, stack offset or call count. Examples: the startup GIF paths at `0x28B96C`/`0x28B978`/`0x28B98C` and `0x3101A8`/`0x310148`/`0x310194` in `ps2_runtime_memory.cpp`, and the phase-locked thread syscalls in `ps2_runtime_syscalls.cpp`.
- **Classify each one:** general hardware or kernel behavior verified at more than one site; behavior understood at only one site; or not yet understood.
- **Generalize only the first group.** Each generalization gets a regression test and keeps the comparisons passing. Leave the others as checked boundaries, and record why in the audit.
- Record the audit list and its outcome in the monthly history.

### 3. GIF DMA chain mode (current blocker)

- **Evidence first.** The routine at `0x312060` reads TADR (`lw` at `0x312084`), rewrites it unless it reads `0xFFFFFFFF`, then reads CHCR, clears its MOD bits (mask `0xFFFFFFF3`) and zeroes QWC. The transfer mode it finally starts is not confirmed yet. Follow the routine to its CHCR start and record the final CHCR (MOD, TTE, TIE), TADR, MADR/QWC and, for chain mode, the tag chain in RAM (tag IDs, QWC, addresses, any tag IRQ bit). Capture the original at the same point if the ordering or completion behavior is unclear. Implement chain mode only if that start uses it.
- **One chain walker.** VIF1 already walks chains inside `frameVif1Start` (`ps2_runtime_memory.cpp`): refe/cnt/next/ref/refs/end, tag transfer, and call/ret and tag IRQs refused. Extract it into a shared walker used by both channels; do not write a second one. GIF differs only in where the payload goes (the GS packet path) and in tag transfer.
- **Behavior to keep or add:**
  - tag IDs as reached, with call/ret (ASR stack) only if reached;
  - transfer ordering, with PATH3 after the preceding VIF1 PATH2 work;
  - QWC/MADR/TADR updates exactly as the channel leaves them;
  - completion (STR clear, D_STAT channel bit);
  - interrupts (tag IRQ and TIE, CIS) only if the game enables them.

  Data with the scratchpad bit (bit 31) is not handled today, since tag addresses are masked with `0x7FFFFFF0`. Handle it if the chain uses it.
- **Tests:** a unit test per reached tag type, a register state check after each transfer, and VIF1's existing tests still passing through the shared walker.
- **References:** PCSX2 `Dmac.cpp`/`Gif.cpp`; PS2Recomp's GIF arbiter for path ordering; [paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference) for the GS side of the packets.

### 4. Opening movie

IPU MPEG-2 decoding needs `docs/plans/video/IPU_PLAN.md` before any implementation.

## Verification and pass completion

A pass is complete only when all of these hold:

1. **Debug gate:** ALL and `bt2-entry-probe` build, **18/18 CTests**, and the Python suites (capture, recompilation, IOP, Ghidra) pass. The sound-worker diagnostic stays at 302/295.
2. **Original comparisons:**
   - **42 of 46 pass**: 23 on the full run, 12 at the four phase stops (`BT2_PHASE_STOP` = `311860`, `310518`, `28b218`, `24b81c`), and 7 with their own inputs (commands below).
   - Known failures: `cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`. A pass may not add failures; a change in these needs a recorded reason.
3. **Release rerun:** reaches the same checkpoint as Debug, and a Release frame render is checked visually.
4. **Documentation:** PROG.md (numbers and statuses), the monthly history (measurements, audit, results), this handoff with the next actionable step, `components.json` if a status changed, `sync_progress.py --check`, links resolving, and no mojibake.
5. **Commit and push**, with no attribution lines.

The full gate is a scratch script, not a tracked tool; its steps are the list above.

### Comparisons outside the gate script

Run these with their own inputs (`$C=work/pcsx2-capture`, outputs to a scratch folder): `verify_cd_init.py $C/cd-init-after/capture.json --semaphores-capture $C/cd-semaphores/capture.json`; `verify_rpc_bind.py $C/sif-bind-after/capture.json --call-sema-capture $C/rpc-call-sema/capture.json`; `verify_heap_bind.py`, `verify_heap_allocation.py` and `verify_heap_copy.py` with `--capture $C/fileio-bind-after|heap-alloc-after|heap-copy-after --disc <ISO> --output <scratch>`; `verify_spu2_transfer.py --disc <ISO>`; `verify_disc_read.py --disc <ISO> --native work/bt2-generation/sound-worker-stop.json --workspace 0x1D4B00`. `verify_rpc_bind.py` and `verify_cd_init.py` rewrite their probe inputs in `work/bt2-generation/`; back them up first and confirm they are unchanged.

## Deferred follow-ups (not started)

- **After the movie:** the title screen waits for Start; without input the game runs a CPU-vs-CPU demo fight (user report).
- **Frame field:** handler-delivered VBlanks while main sleeps do not toggle `display_field`. Check against an original capture if a field-dependent path diverges.
- **Memory card, host input (XInput/SDL, keyboard), 16:9:** unchanged.
- **Map-gap iterations** rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` could be split by subsystem.
- **Loop and gate tooling:** the selection-loop driver, receipt refresh and full gate still live in scratch scripts; move them into `tools/`.
