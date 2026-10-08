# Resume handoff

Updated: 2026-10-08. **No pass in progress.**

The verified checkpoint is **772 entered / 763 completed** (106 frames, boundary `FUN_0026F888`), measured on the reorganized runtime. Its history is in [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../RUNTIME_LAYOUT.md).

## Next implementation

1. **Selection loop:** continue from `FUN_0026F888`.
2. **Scripted pad input:** without a memory card, the game shows a notice that waits for a button press (user note). Add a deterministic frame-indexed button script to the DualShock 2 model (for example `BT2_PAD_SCRIPT`), and confirm the screen with a PNG first.
3. **Host input:** XInput/SDL and keyboard for the live window.
4. **Optimized build:** for real-time viewer speed.
5. **Later (requested):**
   - save import from emulator card images, real-console exports and internet formats ([memory card plan](../MEMORY_CARD_PLAN.md));
   - 16:9.

## Possible follow-up refactors (not started)

- **`tests/bootstrap_tests.cpp`:** 1,489 lines; it could be split by subsystem.
- **`ee/ps2_runtime_syscalls.cpp`:** `handleSyscall` is one 840-line dispatcher; it could be split per service family.
