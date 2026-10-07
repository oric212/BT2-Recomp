# Resume handoff: asynchronous FILEIO for DBZP.BIN

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **595 entered / 587 completed**, kernel 2/2, 92,247 calls / 92,184 returns, 198,954 IOP instructions / 248 targets; FILEIO bound and initialized (one request). Boundary: FILEIO open (function 0, 0x418 bytes, `cdrom0:\BIN\DBZP.BIN;1`) from main's first frame step, validated on the EE side and stopped before the IOP is entered.

## Next implementation

Follow the findings and next steps in [EE_FILEIO_PLAN.md](../EE_FILEIO_PLAN.md): trace FILEIO's per-request worker and the IOP→EE SIF command it sends, model that delivery into the C library's EE handler, place the 0xC40-byte request record, and serve IOMAN `cdrom0:` from the local ISO. Original evidence: `rpc-dbzp-loaded` (DBZP.BIN at EE `0x3B3100`; FILEIO read buffer at IOP `0x1E7100`). The SOUNDS hardware timer has not ticked yet in normal startup. The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest. Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
