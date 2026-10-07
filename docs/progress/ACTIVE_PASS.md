# Resume handoff: streamed CDVDFSV reads

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **564 entered / 556 completed**, kernel 2/2, 80,978 calls / 80,912 returns, 159,101 IOP instructions / 248 targets, 42 CRI calls, 7 heap calls / 2 frees, 15 module loads, 109 logical VBlanks + 2 polled, 16 Timer 2 overflows, 54 original disc reads. Boundary: helper 6's NCMD read of 52 sectors (LBN 0x20544 → EE 0x779440), stopped before the IOP is entered.

## Next implementation

Requests above 16 sectors use the original handler's chunk loop (`0x1A0EF0`–`0x1A1834`): one `sceCdRV` with cache count 16 and callback `0x1A0B00`, then per chunk a header check (`0x1A0B28`), CDVDMAN sync/position commands (ordinal 11 with mode 32, S-command -24) and a SIF DMA of each chunk. The owned read model supplies one transfer and one completion. Model the CDVDMAN ring stream in logical order (sectors from the local ISO, per-chunk completion), compare the EE destination with the ISO and, if needed, capture the original after the read. Timer 2 and the file-load idle site are complete ([timer plan](../EE_TIMER_PLAN.md)). The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest: the `generated-entry-probe` test rewrites `entry-stop.json`. Comparisons added this pass: `verify_timer2.py` (capture `rpc-timer-seed-after`). Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
