# Resume handoff: frame GIF DMA

Updated: 2026-10-07. **Pass verified and committed; next subsystem planned, not started.**

Normal checkpoint: **608 entered / 597 completed** (two overlay entries), kernel 2/2, 92,401 calls / 92,339 returns, 203,146 IOP instructions / 284 targets.
- The overlay frame path ran its first display flip: PMODE `0x66`, DISPFB2 `0x1000`, DISPLAY2 for 512×448.
- Timer 1 is started; its H-BLANK wait has not been reached yet.
- Boundary: the GIF channel-2 idle read at `0x3100E4` in the draw-environment send (`0x3100D0`), a checked memory stop.

## Next implementation

Follow [FRAME_GIF_PLAN.md](../FRAME_GIF_PLAN.md):
1. general channel-2 normal-mode transfers in main's thread;
2. a GIF PACKED/REGLIST/IMAGE parser;
3. a GS register file with recorded primitive kicks;
4. verify each packet against an original capture at the send.

## Overlay pipeline

Overlay map gaps go through these steps:
1. `analyze_game.py --discover-blocker`, which follows the blocker's `program`;
2. `prepare_map.py --program dbzp`;
3. `generate_game.py --program dbzp --candidate-map work/bt2-generation/dbzp/candidates.csv`;
4. the usual `expand_startup.py` and `prepare_runtime.py`.

Extract the overlay once with `overlay_programs.py --extract <ISO>`.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest.

- New comparison: `verify_overlay_dispenv.py`.
- Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
- Still open: the SOUNDS hardware timer has not ticked in normal startup, and the IOP stream consumer acknowledgement is unmodelled.
