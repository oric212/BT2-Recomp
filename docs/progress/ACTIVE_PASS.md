# Resume handoff: CRI command service after the first archive load

Updated: 2026-10-07. **Previous pass verified and committed; next pass not started.**

Normal checkpoint: **510 entered / 499 completed**, kernel 2/2, 74,373 calls / 74,359 returns, 138,268 IOP instructions, 37 CRI calls, six heaps, 105 logical VBlanks, 53 original disc reads. Boundary: main's SifSetDma for CRI command `0x408` (client 0x355C70, RPC ID 0xFA, send 16 bytes from 0x39DC80, result 0x39DD80), the 38th CRI call. Main is inside the shared critical section (priority 1, helper 4 resumed).

## Next implementation

The five second-channel `0x408` handlers are already validated in the isolated native service test. Admit them in normal startup at this later phase through the existing CRI receiver, then follow main's continuation. The IOP stream consumer acknowledgement (trailer counters written back to 2 in the original) is still unmodeled; capture its IOP thread before supplying it. `div.s` is emitted inline with IEEE semantics; patch it in prepared sources if reached.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. New original captures: `rpc-vblank-first`, `rpc-load-poll-idle`, `rpc-load-poll-done` (`tools/capture_ee/capture_ee.py`). Comparison: `tools/capture_ee/verify_vblank_wake.py`. `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py` and `verify_second_heap.py` remain earlier-phase checks.
