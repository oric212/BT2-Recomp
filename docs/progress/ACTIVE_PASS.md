# Resume handoff

Updated: 2026-10-09. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames). The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Shrink the PS2 layer before extending it: one general implementation of each behavior BT2 uses, built by simplifying duplicated logic, generalizing verified special cases and reusing existing code, not by adding blocker-specific patches. Then reach the opening movie on top of that. The order follows "Current next step" in [the implementation plan](../project/IMPLEMENTATION_PLAN.md#current-next-step-change-of-strategy): profiling → handler audit → GIF DMA investigation. Every change needs tests and passing PCSX2 comparisons; no speculative features or rewrites.

### 1. Profiling baseline (Release)

- **Reuse** the probe report's counters (GS pixels, sprites and triangles, VIF1 transfers, IOP instructions `native_iop_steps`, EE calls, VBlanks). The earlier GS figures (92% of the run before optimization, about 75% after) came from a temporary in-process sampler that is not in the repository.
- **Add only** per-subsystem wall time (GS rasterization, GIF/VIF, IOP execution, scheduler/VBlank, report writing), behind an opt-in switch and outside `ps2_runtime.h`.
- **Record** the baseline in the monthly history (workload: default startup run, 650 frames; build; commit; total time; share per subsystem), repeated for stability. No optimization in this step.

### 2. Handler audit and consolidation

- **List** handlers in `runtime/ee/` and `runtime/iop/` gated on a guest PC, stack offset or call count, for example the startup GIF paths at `0x28B96C`/`0x28B978`/`0x28B98C` and `0x3101A8`/`0x310148`/`0x310194` (`ps2_runtime_memory.cpp`) and the phase-locked thread syscalls (`ps2_runtime_syscalls.cpp`).
- **Consolidate duplicates.** GIF channel 2 has four separate models: `gif_dma` and `clear_repeat_gif` (`GifDmaStaging`), `texture_gif` (`TextureGifStartup`) and `frame_gif`. VIF1 has two (`vif_startup`, `frame_vif1`). Fold the verified behavior of each into one model.
- **Generalize** only behavior verified at more than one site. Behavior understood at one site, or not understood, stays a checked boundary, with the reason recorded.
- **Prove each change** with a regression test and unchanged comparisons before removing the old path. Record in the monthly history what was removed, generalized or kept.

### 3. GIF DMA investigation (current blocker)

- **Evidence first.** The routine at `0x312060` reads TADR (`lw` at `0x312084`), rewrites it unless it reads `0xFFFFFFFF`, then reads CHCR, clears its MOD bits (mask `0xFFFFFFF3`) and zeroes QWC. Follow it to its channel start and record the final CHCR (MOD, TTE, TIE), TADR, MADR/QWC and, if chained, the tag chain in RAM (IDs, QWC, addresses, tag IRQ bits). Capture the original at that point if ordering or completion is unclear. Implement chain mode only if that start uses it.
- **Extend the consolidated GIF model from step 2.** For chain mode, extract the existing VIF1 walker in `frameVif1Start` (refe/cnt/next/ref/refs/end, tag transfer; call/ret and tag IRQs refused) into one walker shared by both channels. GIF differs only in the payload destination (the GS packet path) and tag transfer.
- **Required behavior:** tag IDs as reached (call/ret only if reached); PATH3 after the preceding VIF1 PATH2 work; QWC/MADR/TADR left as the hardware leaves them; completion (STR clear, D_STAT bit); interrupts (tag IRQ, TIE, CIS) only if enabled. Scratchpad-sourced data (bit 31, currently masked off by `0x7FFFFFF0`) only if the chain uses it.
- **Tests:** a unit test per reached tag type, register checks after each transfer, VIF1's tests passing through the shared walker, and a comparison with an original capture before the transfer counts as verified.
- **References:** PCSX2 `Dmac.cpp`/`Gif.cpp`; PS2Recomp's GIF arbiter for path ordering; [paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference) for the GS side of the packets.

### 4. Opening movie

IPU MPEG-2 decoding needs `docs/plans/video/IPU_PLAN.md` before any implementation.

## Pass completion

1. **Debug gate:** ALL and `bt2-entry-probe` build, **18/18 CTests**, the Python suites (capture, recompilation, IOP, Ghidra) pass, and the sound-worker diagnostic stays at 302/295.
2. **Original comparisons:** **42 of 46 pass**: 23 on the full run, 12 at the phase stops (`BT2_PHASE_STOP` = `311860`, `310518`, `28b218`, `24b81c`) and 7 with their own inputs (below). Known failures: `cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`. No new failures; any change to these needs a recorded reason.
3. **Release rerun:** same checkpoint as Debug, with a frame render checked visually.
4. **Docs:** PROG.md, the monthly history (baseline, audit, results), this handoff with the next actionable step, `components.json` if a status changed, `sync_progress.py --check`, links resolving, no mojibake.
5. **Commit and push**, with no attribution lines.

The full gate is a scratch script, not a tracked tool; its steps are this list.

### Comparisons outside the gate script

Run these with their own inputs (`$C=work/pcsx2-capture`, outputs to a scratch folder): `verify_cd_init.py $C/cd-init-after/capture.json --semaphores-capture $C/cd-semaphores/capture.json`; `verify_rpc_bind.py $C/sif-bind-after/capture.json --call-sema-capture $C/rpc-call-sema/capture.json`; `verify_heap_bind.py`, `verify_heap_allocation.py` and `verify_heap_copy.py` with `--capture $C/fileio-bind-after|heap-alloc-after|heap-copy-after --disc <ISO> --output <scratch>`; `verify_spu2_transfer.py --disc <ISO>`; `verify_disc_read.py --disc <ISO> --native work/bt2-generation/sound-worker-stop.json --workspace 0x1D4B00`. `verify_rpc_bind.py` and `verify_cd_init.py` rewrite their probe inputs in `work/bt2-generation/`; back them up first and confirm they are unchanged.

## Deferred follow-ups (not started)

- **After the movie:** the title screen waits for Start; without input the game runs a CPU-vs-CPU demo fight (user report).
- **Frame field:** handler-delivered VBlanks while main sleeps do not toggle `display_field`. Check against an original capture if a field-dependent path diverges.
- **Memory card, host input (XInput/SDL, keyboard), 16:9:** unchanged.
- **Map-gap iterations** rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` could be split by subsystem.
- **Loop and gate tooling:** the selection-loop driver, receipt refresh and full gate still live in scratch scripts; move them into `tools/`.
