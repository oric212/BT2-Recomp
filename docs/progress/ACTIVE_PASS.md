# Resume handoff: remaining second-channel CRI pairs

Updated: 2026-10-07. **Previous pass verified and committed; next pass not started.**

Normal checkpoint: **511 entered / 502 completed**, kernel 2/2, 74,419 calls / 74,411 returns, 141,526 IOP instructions / 247 targets, 38 CRI calls, six heaps, 105 logical VBlanks, 53 original disc reads. The first late `0x408` pair completed through the original IOP handler, admitted against the CRI client's live RPC ID. Boundary: mapped generated entry `0x2FFA00` (source 0x2F3C20), deliberately not selected in this verification-only pass.

## Next implementation

Select `0x2FFA00` with the existing tooling and continue the remaining four second-channel pairs; `verify_cri_second_channel.py` compares partial progress against the next original request capture. The IOP stream consumer acknowledgement is still unmodeled. Read-only tracing points to resident CRI worker `0x1A7088` (priority 39), which waits for IOP VBlank end, distinct from the AutoDMA child. Original slot counters are 2 at the first EE VBlank and 238 after loading; native counters stay zero. Shared IOP VBlank delivery would also wake SIO2D/DS2UD. Do not fabricate the wake or acknowledgement. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Late-pair admission fixtures: `runtime/iop/ee_bridge_tests.cpp`. Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
