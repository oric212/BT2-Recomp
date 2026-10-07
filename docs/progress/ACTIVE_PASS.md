# Resume handoff: DBZP.BIN overlay

Updated: 2026-10-07. **Pass verified and committed; next subsystem planned, not started.**

Normal checkpoint: **601 entered / 595 completed**, kernel 2/2, 92,392 calls / 92,336 returns, 203,146 IOP instructions / 284 targets. FILEIO: one bind, six calls (initialization, open, two seeks, read, close), five worker completions. `DBZP.BIN` is loaded byte-identically to the original. Boundary: main calls `0x3B7FA8` inside the loaded overlay (map gap outside the executable).

## Next implementation

Follow [EE_OVERLAY_PLAN.md](../EE_OVERLAY_PLAN.md):
1. wrap the locally extracted, hash-pinned `DBZP.BIN` at `0x3B3100`;
2. analyse it and prepare a separate overlay map;
3. generate it into ignored output;
4. register it behind a load guard on the loaded bytes;
5. teach the selection loop to classify overlay targets.

Still open from earlier passes:
- The SOUNDS hardware timer has not ticked in normal startup.
- The IOP stream consumer acknowledgement is unmodelled.
- `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest.

- New comparison: `verify_dbzp_load.py`.
- Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
