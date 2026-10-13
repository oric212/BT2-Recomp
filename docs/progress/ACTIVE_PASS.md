# Resume handoff

Updated: 2026-10-13. **No pass in progress.**

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. Every mapped function (10,680) is compiled into the probe. The long Release run with the movies skipped shows the title screen and the game's idle cycle (demo fight, title, second demo fight) in 3D and ends at its 9,000-frame budget (4,854 / 4,847 functions). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Menus and battle setup, per [PROG.md](../PROG.md#next-steps):

1. Find the input that leaves the title screen. The pad script's Start presses (every 150 fields from 1100 to 4000) skip the movies, but the frames afterwards show the title/demo cycle, never the New Game/Continue choice. Check the press timing against the title screen's own frames (a snapshot every 50 frames around the title) and the pad data the game reads.
2. Follow New Game through its boundaries with the discovery loop.
3. Verify the 3D output against PCSX2: a VU1 memory capture point in `capture_ee.py`, and a GS dump or screenshot of a demo frame.

## Frames to inspect

Saved under `work/frames/` (local, ignored), from the long Release run on the GPU renderer:

- `demo-3450.png`, `demo-4300.png`: first demo fight (Yamcha against a Saibaman, rocky plain).
- `demo-5600.png`: title screen between the fights.
- `demo-7000.png`, `demo-8300.png`, `demo-8990.png`: second demo fight (Goku against Ginyu, Namek).

The user knows how the game should look; ask them to confirm these before the demo fight is called Verified.

## How to run the long run

- Movies skipped (about 3.5 minutes to 9,000 frames): `run_startup_probe.py --config Release --frames 9000 --pad-script "<default>,1100:start,1108:-,1250:start,1258:-,..."` (Start every 150 fields from 1100 to 4000).
- Frames: `BT2_GPU_GS_SNAPSHOT="<flip>:<path>;<flip>:<path>"` writes those flips as PPM.
- A hang (no display flip): `BT2_STALL_FIELDS=120` ends the run within seconds; `stop_returns` in the report is the call chain. A bad float: `BT2_EE_WATCH_FLOAT=<address>:<limit>` names the first call after the write ([TOOLS.md](../guides/TOOLS.md)).
- Discovery loop: with every mapped function bound, a stop at an unbound entry is a map gap: `inspect_blocker.py`, `analyze_game.py --discover-blocker` (or `--discover-prologues` for a bulk pass; add `--program dbzp` for the overlay), `prepare_map.py`, `generate_game.py --candidate-map ...`, refresh the receipt, `expand_startup.py --all`, `prepare_runtime.py`, build. This pass drove it with a scratch script (run, classify, discover, regenerate, rebuild, repeat, with a live log); it is worth adding to `tools/ps2recomp/` as a tool with tests. When a tail jump's target becomes a function of its own, the raw source of the jumping function changes (dispatch call to direct call): check the diff, then refresh its hash in the receipt.
- A candidate the prover rejects ("map did not grow"): trace which rule of `has_branch_nop_gaps` / `has_switch_case_gaps` returns, read the code at that address and generalize the rule with a test.

## Notes

- Build with `python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe` (Ninja). Times: runtime header (`ee_runtime.h`) edit about 16 s, one file about 12 s, full rebuild about 6 minutes (only after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`). A change to a rewrite in `prepare_runtime.py` recompiles the generated files it touches (about 200 files: 2 minutes).
- Gate: `python tools/gate/run_gate.py --disc <iso>` (about 80 s plus the build).
- The 3D frames exist only on the GPU renderer: the CPU GS model refuses perspective texturing and fog, so `--gs cpu` and `--gs mirror` stop at the first 3D frame. The gate's CPU run ends before any 3D frame.
- VU1 counts one cycle per instruction pair without stalls. The report's `vu1_early_q_reads` and `vu1_early_clip_reads` count reads that fell inside a pipeline delay; both are 0 in the long run. A nonzero value means a program depends on timing the model does not have.
- SOUNDS functions `0x60`, `0x80` and `0x100` are answered with the unchanged status block by inference (not in the PCSX2 call log); capture them if a difference shows.
- The gate runs 22 comparisons on the full run and 9 at phase stops; the 15 that need their own inputs or an earlier stop, or that differ as recorded in [PROG.md](../PROG.md#verification-2026-10-13), are listed in `NOT_IN_GATE` in `tools/gate/run_gate.py`.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/` (stream captures and logs), `work/pcsx2-capture/sounds-log/` and `sounds-function-*`; reference movie frames in `work/movie-ref/`; the five VU1 microprograms of the first demo list in `work/vu1-programs/`.
