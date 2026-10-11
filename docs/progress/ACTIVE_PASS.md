# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames), now on native IOP services with the emulated IOP removed. The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Opening movie: native IPU decoding ([IPU plan](../plans/video/IPU_PLAN.md#progress-2026-10-12)). The movie's bitstream reaches the native IPU and its sequence header is parsed with FDEC; the decoder thread (7) then waits in the loop at `0x31A3C8` without issuing another command, until the execution budget ends.

1. Read the wait loop at `0x31A3C8` (it polls IPU_BP, IPU_CMD BUSY and IPU_CTRL bit 14) and find the condition it expects after the header.
2. Implement VDEC and the macroblock commands as the movie reaches them, with tests; compare decoded frames with PCSX2.
3. GPU rendering: a PCSX2 GS dump still decides the one-step rounding differences (user).

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-11) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
