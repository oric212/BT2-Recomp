# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames), now on native IOP services with the emulated IOP removed. The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Opening movie ([plan](../plans/video/SOFDEC_PLAN.md)). GIF chain mode is done; the run stops at the first CRI ADX stream commands (commands 8 and 5 on object `0x1B5E2C`, IOP buffer `0x191740`).

1. Capture the original's reply in PCSX2 (installed locally, with the BIOS the earlier captures used) with a new `capture_ee.py` point after the commands are uploaded and answered.
2. Implement the replies in the CRI service with tests; run to the next boundary and record the movie path (IPU or library).
3. GPU rendering: a PCSX2 GS dump still decides the one-step rounding differences (user).

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-11) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
