# Resume handoff

Updated: 2026-10-08. **No pass in progress.**

The verified checkpoint is **801 entered / 791 completed** (649 frames), stopping at a frame-phase GS reset (`0x30FE44`, checked boundary `frame_gs_reset`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../RUNTIME_LAYOUT.md).

## Next implementation

1. **Frame-phase GS reset (separate subsystem).** After the memory card prompts the game resets the GS for its next scene. Keep it isolated from performance work, as the user asked:
   - Model it from an original capture at that reset (new capture point), compare GS/display state and VSync handler changes, and replace the checked boundary only once the comparison holds.
   - The existing startup reset model (`displayResetStep`, phases 0-13) is phase-locked and must stay as is.
2. **Selection loop:** use `run_startup_probe.py --config Release` (about 20 s per iteration).
3. **Memory card:** the game sees no card by design (`sio2_card_absent`). A mounted card needs a blank formatted card image first, then save import ([plan](../MEMORY_CARD_PLAN.md)).
4. **Host input:** XInput/SDL and keyboard for the live window. Scripted input already exists (`--pad-script`).
5. **Later:** 16:9.

## Possible follow-ups (not started)

- **GS rasterizer:** about 75% of a Release run. Per-pixel addressing (`ct32Word`, `t8Byte`/`t4Nibble`) is the remaining cost.
- **Map-gap iterations:** these rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` (1,489 lines) could be split by subsystem.
