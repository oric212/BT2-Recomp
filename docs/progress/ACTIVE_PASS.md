# Resume handoff: EE file I/O through IOP FILEIO

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **591 entered / 582 completed**, kernel 2/2, 92,201 calls / 92,138 returns, 198,905 IOP instructions / 248 targets, 42 CRI calls, 9 heap calls / 2 frees, 15 module loads, 117 logical VBlanks + 2 polled, 17 Timer 2 overflows, 57 original disc reads, three EE→IOP uploads, three sound-driver and two SOUNDS commands; SOUNDS hardware timer started (no tick yet). Boundary: main's first frame step (`0x1002D8`) opening `cdrom0:\BIN\DBZP.BIN`; stdio initialization binds FILEIO server `0x80000001` (client `0x3AC740`).

## Next implementation

Follow [EE_FILEIO_PLAN.md](../EE_FILEIO_PLAN.md): translate boot FILEIO's RPC server (bind, open, lseek, read, close) from the image already used for its heap handler, serve the `cdrom0` device path, and add EE receivers for the C library's FILEIO calls. The SOUNDS hardware timer will tick at the next logical VBlank; its per-tick worker path is untested in normal startup. The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest. Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
