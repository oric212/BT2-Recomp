# Resume handoff: main's post-load setup

Updated: 2026-10-07. **Pass verified and committed; next pass not started.**

Normal checkpoint: **565 entered / 557 completed**, kernel 2/2, 83,789 calls / 83,734 returns, 165,606 IOP instructions / 248 targets, 42 CRI calls, 7 heap calls / 2 frees, 15 module loads, 111 logical VBlanks + 2 polled, 16 Timer 2 overflows, 55 original disc reads. Boundary: mapped generated entry `0x2A2358`, reached from `0x2A0618` after main's first synchronous file load and helper 6's streamed read.

## Next implementation

Select `0x2A2358` with the existing loop (`inspect_blocker.py` → `expand_startup.py` → `prepare_runtime.py` → rebuild `bt2-entry-probe` → `run_startup_probe.py`) and continue through main's post-load setup (`0x102260` onward) toward its frame loop at `0x100680`. Streamed reads use the retail CDVDMAN block layout (12-word blocks, 43 per sector) and one callback per ring chunk; capture the original after a streamed read if a later consumer depends on chunk timing. The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest (the `generated-entry-probe` test rewrites `entry-stop.json`). Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
