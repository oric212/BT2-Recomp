# Implementation plan

Target: *Dragon Ball Z: Budokai Tenkaichi 2*, North American PS2 release SLUS-21441, running the original game logic natively on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20 and CMake.

**Goal:** a native Windows x64 port with a minimal compatibility layer, not a PS2 emulator. The game's EE code is statically recompiled to C++. Underneath it, the PS2 hardware is replaced by native services rather than emulated: host file access for the disc, native audio for the sound modules, host input for the controller, host storage for the memory card, and GPU rendering through paraLLEl-GS. The first major gameplay goal is a playable versus battle.

## Direction (decided 2026-10-11)

| Area | Decision |
| --- | --- |
| Game code | Keep the recompiled EE code as the base. Replace individual functions with hand-written C++ only where it pays off (hardware-facing code, hot loops), through the function table. No full decompilation. |
| IOP side | **Native RPC services** answer the game's SIF RPC requests (disc commands, FILEIO and heap, controller, memory card, SOUNDS, sound driver, CRI ADX) with host code. Done 2026-10-11; the emulated IOP is removed. |
| Graphics | Keep consuming the game's own GIF and VIF packets; render them on the GPU with **[paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)** (Vulkan). The CPU renderer stays as the fallback for systems without a suitable Vulkan GPU (for example some integrated graphics) and as the test reference. VU1 programs, when reached, are handled one by one (shaders or a small interpreter), as [Tenkaichi3Decomp](../reference/REFERENCE_ARCHITECTURE.md#tenkaichi3decomp-native-port-reference) did. |
| Movies | Plan native Sofdec (`.SFD`) playback, video plus ADX audio, instead of emulating the IPU. |

## Architecture rules

### Minimal layer

- **Native service over emulation.** Where the game asks a PS2 service for a result (a file, a sound, pad state, card data), produce that result natively. Emulate hardware only where the game itself drives it directly and no service boundary exists (EE DMA to the GS, VIF packets, the EE kernel).
- **Choose the highest stable boundary.** Prefer the SIF RPC service or the EE library call over the hardware registers below it. Use the boundary that BT2's evidence shows clearly.
- **Implement what BT2 requires.** Add behavior only when BT2 reaches it, in the simplest native form. Do not implement unreached PS2 features.
- **One implementation per behavior.** Merge duplicated models (GIF channel 2 has four today, VIF1 two). Generalize verified special cases instead of adding handlers gated on guest PCs, stack offsets or call counts.
- **Remove what a replacement makes obsolete**, once its tests and comparisons pass.

### Correctness

- **Game-visible results must match.** A native service must leave the same results in EE memory as the original (file bytes, RPC replies, structures the game reads). Compare EE-visible state and presented frames with PCSX2 captures of the original game.
- **Fail explicitly.** Unsupported requests stop at a checked boundary that names them; never return fabricated success.
- **Tests:** a unit test for each service request type and each generalized behavior. Comparisons of IOP internals are retired with the IOP code they checked; record which.
- **Status vocabulary** of [PROG.md](../PROG.md): ✅ Verified, 🟢 Operational, 🔧 Incomplete, ⛔ Blocking, ⏳ Not Reached. Never infer gameplay performance from startup progress or a first-frame milestone.

### Performance

- Measure Release builds on representative workloads before optimizing. The CPU GS rasterizer took about 75% of the 650-frame startup run; the GPU path replaces it. IOP execution (about 10.9 million instructions per startup) disappears with the native services.
- Record before/after measurements in the monthly history; report time per frame or share of run time, never projected gameplay performance.

## Milestones

| Milestone | Status | Evidence |
| --- | --- | --- |
| Native foundation, local game validation, ELF inspection | ✅ Verified | Retail revision confirmed (`config/retail_revision.json`) |
| PS2Recomp generation and integration | ✅ Verified | Provisional map, local generation, compile/link/execute |
| Boot and startup to the main frame loop | ✅ Verified | Original comparisons; see [PROG.md](../PROG.md) |
| First presented frame; controller and memory card checks | ✅ Verified | 650 frames before the movie; 1,000 with its first fields |
| Opening movie | ⛔ Blocking | First pictures decoded natively and presented; playback waits for audio progress |
| Title screen, demo fight, menus | ⏳ Not Reached | |
| Character selection and battle loading | ⏳ Not Reached | |
| Playable versus battle | ⏳ Not Reached | First major gameplay goal |
| Audio and a full match | ⏳ Not Reached | |

## Current next step

Each step keeps the game running to the current checkpoint (650 frames, then the GIF stop) with game-visible results unchanged.

1. **Native services** — done 2026-10-11: every reached IOP service is answered natively and the emulated IOP is removed ([plan](../plans/iop/NATIVE_SERVICES_PLAN.md)).
2. **Native audio output** — later: ADX decoding and sound bank playback behind the sound and CRI services, after the movie needs it.
3. **GPU rendering** ([plan](../plans/graphics/GPU_RENDERING_PLAN.md)): paraLLEl-GS is integrated: mirror mode, scanout presentation and GPU-only mode (startup run 2.3 s instead of 19.1 s). Frames 1–29 match the CPU renderer exactly; later frames differ by one-step rounding in a few hundred words. Next: a PCSX2 GS dump to decide those, then make the GPU the default, with the CPU renderer as fallback.
4. **GIF DMA chain mode** — done 2026-10-11: frame-phase GIF channel 2 runs source chains through the DMA chain walker shared with VIF1. Still open: merging the startup GIF channel-2 models into the frame one.
5. **Opening movie** ([plan](../plans/video/SOFDEC_PLAN.md), [IPU plan](../plans/video/IPU_PLAN.md)), the current blocker: CRI stream replies are native; the native IPU decodes the movie's MPEG-2 pictures (FDEC, VDEC, BDEC, CSC) and the game presents them. Next: the audio playback progress the movie player waits for, then frame comparison with PCSX2.

## Working method

### What "continue" means

A plain "continue" (including spelling/case variants) runs a goal-driven pass, as AGENTS.md defines. Work through "Current next step" above, in order, toward the next functional milestone. Each item follows: inspect → gather evidence → implement the native replacement or general behavior → tests and original comparisons → Release rerun → next item. Finishing one item is not the end of a pass; advance to the next one.

A new execution boundary is input to the plan: decide whether it belongs behind a native service or needs general EE-side behavior, and implement it accordingly. If it opens a new major subsystem, write its plan first (in `docs/plans/<subsystem>/`) and continue when it is actionable. Stop only when required information or access is unavailable, or when the next change would be speculative.

When more evidence is needed, use local captures, the tooling, [the reference architecture](../reference/REFERENCE_ARCHITECTURE.md) and its linked repositories. Adapt reference behavior to BT2's observed behavior; never copy implementations 1:1, decompiled game code, game-specific patches or unsupported success defaults.

### Evidence and content rules

- Keep game content in ignored `rom/` or `game_data/`, analysis output in ignored `work/` or `research/`, and generated code in ignored `game/generated/`. Never download or commit proprietary content.
- Record confirmed facts, hypotheses and unknowns separately. Keep unknown function names neutral until evidence supports renaming.
- Mapping and generation stay incremental: provisional function discovery/map → generation → compile/link → execution. New functions are selected when execution reaches them. Neutral naming and provenance remain required.

### End of a pass

Test the changes, update [PROG.md](../PROG.md) and the monthly history in `docs/progress/`, review the diff and staged files, commit and push. Earlier per-pass handoffs are archived in the [October 2026 history](../progress/2026-10.md).
