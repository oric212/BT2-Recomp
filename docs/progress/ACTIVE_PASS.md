# Resume handoff

Updated: 2026-10-08. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../RUNTIME_LAYOUT.md).

## Next implementation

1. **GIF DMA chain mode:** the run stops at a GIF channel 2 TADR access (`0x1000A030`, pc `0x312084`), probably the movie scene's first chain-mode GIF transfer. Compare with [FRAME_GIF_PLAN.md](../FRAME_GIF_PLAN.md); PS2Recomp's GIF arbiter and PCSX2 `Gif.cpp`/`Dmac.cpp` are the references.
2. **Opening movie (FMV):** needs real IPU MPEG-2 decoding (IPU commands beyond setup, IPU DMA 3/4). This is a new major subsystem: write `docs/IPU_PLAN.md` before implementing it.
3. **After the movie:** the title screen waits for Start. Without input, the menu runs an attract-mode CPU-vs-CPU fight (user report), a good no-input target.
4. **Frame field:** handler-delivered VBlanks while main sleeps do not toggle `display_field` (only polled VBlanks and VSync-flag deliveries do). Check this against an original capture if a field-dependent path diverges.
5. **Memory card, host input (XInput/SDL, keyboard), 16:9:** unchanged from the previous handoff.

## Possible follow-ups (not started)

- **GS rasterizer:** about 75% of a Release run.
- **Map-gap iterations:** these rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` could be split by subsystem.
- **Loop tooling:** the selection-loop driver and receipt refresh still live in scratch scripts; move them into `tools/ps2recomp/`.
