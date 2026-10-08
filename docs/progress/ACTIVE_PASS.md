# Resume handoff

Updated: 2026-10-08. **No pass in progress.**

The verified checkpoint is **804 entered / 793 completed** (650 frames). The frame-phase GS reset is implemented and matches the original. The run stops at the unselected function `FUN_002B7938`. History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../RUNTIME_LAYOUT.md).

## Next implementation

1. **Selection loop:** continue from `FUN_002B7938` (`run_startup_probe.py --config Release`, about 20 s per iteration) toward the next scene.
2. **Prompts in captures:** original captures reached only after a prompt use `capture_ee.py --interactive-pad`, with a person pressing the buttons.
3. **Memory card:** the game sees no card by design (`sio2_card_absent`). A mounted card needs a blank formatted card image first, then save import ([plan](../MEMORY_CARD_PLAN.md)).
4. **Host input:** XInput/SDL and keyboard for the live window.
5. **Later:** 16:9.
6. **Viewer flicker (user report, 2026-10-08):** the live window flickers slightly.
   - **Likely cause:** with integer scaling, `ViewerWindow::paint` (`runtime/host/viewer_window.h`) fills the whole client area black before `StretchDIBits`, without double buffering. Fill only the borders, or paint through a memory DC.
   - **To check:** whether presenting both interlaced fields' buffers adds to it.

## Possible follow-ups (not started)

- **GS rasterizer:** about 75% of a Release run. Per-pixel addressing (`ct32Word`, `t8Byte`/`t4Nibble`) is the remaining cost.
- **Map-gap iterations:** these rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` (1,489 lines) could be split by subsystem.
