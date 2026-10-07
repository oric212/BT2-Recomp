# Resume handoff: EE Timer 2 system clock

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **531 entered / 524 completed**, kernel 2/2, 75,544 calls / 75,538 returns, 156,780 IOP instructions / 248 targets, 42 CRI calls, 7 heap calls / 2 frees, 15 module loads, 105 logical VBlanks + 2 polled, 53 original disc reads. Boundary: main's first Timer 2 COUNT read at `0x339430` (system time after display setup).

## Next implementation

Follow [EE_TIMER_PLAN.md](../EE_TIMER_PLAN.md): logical Timer 2 count advanced at VBlank points, overflow/compare flags, cause-11 delivery to handler `0x339118`. Original evidence: `rpc-timer-seed-after` (result `0x1C825400`, overflow word 0x1C). Kernel handler IDs and IOP module IDs are model-local (native one below the original). The IOP stream consumer acknowledgement remains unmodelled (resident CRI worker `0x1A7088` waits for IOP VBlank end). `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest: the `generated-entry-probe` test rewrites `entry-stop.json` with its reduced report. New comparison: `verify_display_reset.py` (captures `rpc-display-init-after`, `rpc-timer-seed-after`). Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
