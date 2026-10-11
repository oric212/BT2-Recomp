# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames), now on native IOP services with the emulated IOP removed. The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

GPU rendering through paraLLEl-GS ([plan](../plans/graphics/GPU_RENDERING_PLAN.md#progress-2026-10-11)). Mirror mode runs: frames 1–29 match the CPU renderer exactly; from frame 30 a few hundred words per frame differ by one step in a color channel.

1. **Needs the user:** a PCSX2 GS dump of the startup frames (through frame ~40), kept in ignored `work/`, to decide which renderer is right for those words.
2. Then: make the GPU the default (move the verifiers that read CPU VRAM to GPU readback) and fall back to the CPU renderer when no suitable Vulkan device exists. Presentation and GPU-only mode (2.3 s startup run) are done.
3. Then GIF DMA chain mode (the current blocker) and the opening movie plan.

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-11) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
