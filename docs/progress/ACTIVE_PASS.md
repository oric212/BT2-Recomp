# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames), now on native IOP services with the emulated IOP removed. The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Opening movie: native IPU decoding ([plan](../plans/video/SOFDEC_PLAN.md#progress-2026-10-12)). The run stops at the movie's first toIPU DMA (channel 4 CHCR `0x1000B400`, pc `0x2CB384`) after 1,590 functions.

1. Write `docs/plans/video/IPU_PLAN.md`: the IPU commands and DMA the movie issues (capture them in PCSX2 with the unattended pad injection if needed), MPEG decoding and CSC output.
2. Implement it as general IPU behavior with tests; compare decoded frames with PCSX2.
3. GPU rendering: a PCSX2 GS dump still decides the one-step rounding differences (user).

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-11) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
