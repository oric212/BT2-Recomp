# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames), now on native IOP services with the emulated IOP removed. The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

GPU rendering through paraLLEl-GS, step 3 of "Current next step" in [the implementation plan](../project/IMPLEMENTATION_PLAN.md#current-next-step):

1. Write `docs/plans/graphics/GPU_RENDERING_PLAN.md`.
2. Build paraLLEl-GS (Vulkan, Granite) with MSVC; it already builds standalone in ignored `work/dependencies`.
3. Feed it the existing GIF/VIF1 output and privileged display registers; present its scanout in the window.
4. Match it against the CPU renderer on the 650 startup frames.

Then GIF DMA chain mode (the current blocker) and the opening movie plan.

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-11) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
