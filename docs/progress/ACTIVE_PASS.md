# Resume handoff: interrupt delivery, wakes and first asynchronous load

Updated: 2026-10-06. **Previous pass verified and committed; next pass not started.**

Normal checkpoint: **429 entered / 420 completed**, kernel 2/2, 4,729 calls / 4,723 returns, 81,640 IOP instructions, 37 CRI calls, six heaps. Boundary: main's first idle call in its asynchronous request poll loop, `0x1007F0 → 0x2EF218` (checked stop; `load_poll_boundaries` = 1).

Main priority 24 is admitted with previous priority 2 returned. Helpers 5/6 are parked at their captured first SleepThread; IDs 4 and 7 are suspended; sound child 3 is sleeping. A VBLANK_START handler (0x1006D8) is registered and cause 2 enabled; no interrupt is delivered.

## Next implementation

Follow [EE_INTERRUPT_LOAD_PLAN.md](../EE_INTERRUPT_LOAD_PLAN.md). First trace the request handle state machine (0x300CD0 and callees) and VBlank handler 0x1006D8 → 0x2EF0D8 against new original captures. Then implement owned VBlank delivery at explicit scheduling points and wakes of retained parked stacks. Do not fabricate request completion or CRI acknowledgements.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` (normal) and `sound-worker-stop.json` (separate diagnostic). Helper comparison: `tools/capture_ee/verify_startup_workers.py`. At this later phase, `verify_disc_read.py` and `verify_spu2_initialization.py` also fail on the previous pass's late reports (reused IOP sector buffer; missing report field). `verify_cri_rpc.py`, `verify_cri_preemption.py` and `verify_second_heap.py` describe earlier checkpoints only. Make those phase-aware in a later tooling pass.
