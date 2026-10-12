# Resume handoff

Updated: 2026-10-13. **No pass in progress.**

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. Every mapped function (10,650) is compiled into the probe. The long Release run with the movies skipped reaches the title screen, chooses New Game and stops at the first VIF1 display list of the 3D pipeline (3,198 / 3,189 functions, 3,385 frames). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

The 3D pipeline ([plan](../plans/graphics/VU1_PIPELINE_PLAN.md)): VIF1 display lists (CHCR `0x145`, tag transfer) and VU1 microprograms.

1. Trace the VIFcodes of the first display lists at the checked boundary (pc `0x28B3E8`) and list UNPACK formats and microprogram uploads.
2. Implement VIF1 with tag transfer, the state commands, MPG and UNPACK, with tests; then the VU interpreter and XGKICK.
3. Ask the user to inspect the first 3D frame; they described the expected flow (title idle → CPU-vs-CPU demo; Start → New Game/Continue; no-card dialogue → menu).

## How to run the long run

- Movies skipped (seconds): `run_startup_probe.py --config Release --frames 9000 --pad-script "<default>,1100:start,1108:-,1250:start,1258:-,..."` (Start every 150 fields from 1100 to 4000).
- Movies played: the same without the Start presses (minutes).
- Discovery loop: with every mapped function bound, a stop at an unbound entry is a map gap: `inspect_blocker.py`, `analyze_game.py --discover-blocker` (or `--discover-prologues` for a bulk pass; add `--program dbzp` for the overlay), `prepare_map.py`, `generate_game.py --candidate-map ...`, refresh the receipt, `expand_startup.py --all`, `prepare_runtime.py`, build.

## Notes

- Build with `python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe` (Ninja). Times: runtime header (`ee_runtime.h`) edit about 16 s, one file about 12 s, full rebuild about 6 minutes (only after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`).
- Gate: `python tools/gate/run_gate.py --disc <iso>` (about 80 s plus the build).
- The user knows how the game should look and offers to inspect frames saved under `work/frames/`.
- SOUNDS functions `0x60`, `0x80` and `0x100` are answered with the unchanged status block by inference (not in the PCSX2 call log); capture them if a difference shows.
- The gate runs 22 comparisons on the full run and 9 at phase stops; the 15 that need their own inputs or an earlier stop, or that differ as recorded in [PROG.md](../PROG.md#verification-2026-10-13), are listed in `NOT_IN_GATE` in `tools/gate/run_gate.py`.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/` (stream captures and logs), `work/pcsx2-capture/sounds-log/` and `sounds-function-*`; reference movie frames in `work/movie-ref/`.
