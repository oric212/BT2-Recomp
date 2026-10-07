# Resume handoff: SPU2 voice state

Updated: 2026-10-07. **Pass verified and committed; next subsystem planned, not started.**

Normal checkpoint: **620 entered / 609 completed** (two overlay entries), kernel 2/2, 92,419 calls / 92,358 returns.

What the first frame has done:
- **Draw environment:** one frame GIF transfer (240 bytes, 14 register writes) cleared 512×448 in the owned VRAM model.
- **Path sync:** the GS library's path sync ran twice.
- **Render kick:** one empty VIF1 chain (1 tag, 2 NOP codes).
- **Display:** 11 display register writes (the library flip plus the game's own setup of both read circuits).
- **Timer 1:** the wait has polled 263 lines.

**Boundary.** The first field boundary inside that wait would deliver the first SOUNDS timer tick. Diagnostically, that tick's worker reads SPU2 ENDX (`0x1F900340`, IOP PC `0x937A4`). Until voice state exists, `sounds_timer_ticks_supported` holds that VBlank as a checked EE boundary (selector `0x14`), before any field or IOP effect.

## Next implementation

Follow [SPU2_VOICE_PLAN.md](../SPU2_VOICE_PLAN.md):
1. per-voice playback positions from pitch, SSA and ADPCM block flags in the owned sound RAM, advanced in logical IOP time;
2. ENDX reads from that model;
3. KON/KOFF transitions.

After that, the Timer 1 wait continues through the rest of the frame.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest.

- `verify_overlay_dispenv.py` now also holds at later checkpoints (memory only; flip registers are compared at the flip).
- Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
- Still open: the IOP stream consumer acknowledgement is unmodelled.
