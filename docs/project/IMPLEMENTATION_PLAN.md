# Implementation plan

Target: *Dragon Ball Z: Budokai Tenkaichi 2*, North American PS2 release SLUS-21441, running the original game logic natively on Windows 10/11 x64. Build with Visual Studio 2022, MSVC, C++20 and CMake.

**Goal:** a native Windows x64 port with a minimal compatibility layer, not a PS2 emulator. The game's EE code is statically recompiled to C++; the PS2 hardware underneath is replaced by native services (disc, sound, controller, memory card) and GPU rendering through paraLLEl-GS. Then optional enhancements that never change how the game plays.

## Direction

| Area | Decision |
| --- | --- |
| Game code | The recompiled EE code is the base. Hand-written C++ only where it pays off (hardware-facing code, hot loops). No full decompilation. An EE interpreter only as a logged bridge for code the map does not hold yet, compiled natively at the next build (2026-10-18, [plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)). |
| IOP side | Native RPC services answer the game's SIF RPC requests (disc, files and heap, controller, memory card, sounds, CRI ADX); no IOP code runs (2026-10-11). |
| Graphics | The game's own GIF and VIF packets, rendered on the GPU by [paraLLEl-GS](../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference); VU1 programs native, the heaviest hand-written. The only renderer; the GS command sink checks the packets on the game's thread (the CPU renderer was removed 2026-10-21), and each renderer takes its data on a GS thread of its own (2026-10-25). |
| Films and sound | The game's Sofdec library decodes the films through a native IPU; ADX streams and a native effects sampler are mixed by the port. |
| Enhancements | Off by default, settings in the launcher and the in-game menu. They must leave the fight's speed, physics and results exactly as in the original (the user, 2026-10-20), and the port must not be held back by the processor or the graphics card. |
| Hardware use | The port uses the machine: work that does not feed back into the game runs on threads of its own (the GS threads, the in-between pictures), and RAM and VRAM hold what avoids waits (HD textures streamed ahead). Emulated work becomes native code where a measurement shows it pays (the user, 2026-10-25). The game's own computation (its lighting, its physics) stays the game's. |

## Architecture rules

- **Native service over emulation**, at the highest boundary BT2's evidence supports; emulate only what the game drives directly (EE DMA, GIF and VIF packets, VU1, the IPU, the EE kernel).
- **Implement what BT2 requires,** in the simplest native form; **one implementation per behavior**; generalize verified special cases instead of handlers gated on guest PCs, stack offsets or call counts; remove what a replacement makes obsolete.
- **Game-visible results must match** the original (EE memory, files, replies, frames, sound); compare byte-exactly where possible. **Fail explicitly** at a checked boundary; never fabricate success.
- **Tests** for each service request type and each generalized behavior; status in the vocabulary of [PROG.md](../PROG.md).
- **Performance:** measure Release builds on representative workloads; report time per frame or share of run time, never projected gameplay performance.
- **Threads:** a renderer is used by one thread at a time; the game's thread waits for a GS thread only where it needs the renderer's result. A threading change is checked unpaced (headless) as well as in the window, and under AddressSanitizer when it crashes.

## Milestones

| Milestone | Status |
| --- | --- |
| Foundation, generation, startup to the frame loop, controller and card checks | ✅ Verified |
| Films, title, menus, all game modes, story mode (all parts and towns without input) | 🟢 Operational |
| Versus battle with a controller, sound, real-time speed | 🟢 Operational |
| Saving | ✅ Verified (byte-identical to the original's) |
| Launcher, settings, in-game menu, release package | 🟢 Operational |
| Enhancements: HD textures, super-sampling, widescreen, 60 fps battles, fast loading, Low input lag | 🟢 Operational |
| Online battle | 🟢 Operational |
| Final validation: the PCSX2 comparison, the exhaustive battle sweep | ⏳ In progress (the fight frame's logic and draw steps match PCSX2) |

## Current next step

Done: native services, GPU rendering, films, the 3D pipeline, sound, controller, all modes without input, performance, the enhancements, the launcher and release package, online battles (history: [October](../progress/2026-10.md), [November](../progress/2026-11.md)).

Next, in order:

1. **The PCSX2 comparison** ([plan](../plans/validation/PCSX2_COMPARISON_PLAN.md); the user, 2026-11-12): steps 1 and 2 done (the fight frame and a full versus battle equal). Next: consecutive frames (step 3), input lag and latency (step 9), loaded data (step 5), then VU1 and the GIF stream, GS memory and pictures, the services, and the comparisons into the gate.
2. **The user's open reports**, as detail comes: the destroyed Namek's flicker at 60 fps ([60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md)); Frieza over the ocean and characters under water with HD ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)). Stops in the user's play are mapped with `discovery_loop.py --player-stop`.
3. **Picture and responsiveness:** an edge filter ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)); 60 fps load work ([native fight rendering plan](../plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md)); responsiveness steps 7 and 8 ([plan](../plans/ee/RESPONSIVENESS_PLAN.md); waiting for the user's go).
4. **Online, left for later** ([online plan](../plans/online/ONLINE_BATTLE_PLAN.md)): the Dragon Rush's prompts; lock-on in play.
5. **Later features:** textures the HD pack lacks upscaled by the port ([plan](../plans/graphics/TEXTURE_UPSCALING_PLAN.md)); the exhaustive battle sweep.
6. **The user's decisions:** a matchmaking server, rollback, publishing the release ([release plan](../plans/launcher/RELEASE_PLAN.md)).

## Working method

- **"Continue"** runs a goal-driven pass as [AGENTS.md](../../AGENTS.md) defines: work through "Current next step" in order: inspect → evidence → native replacement or general behavior → tests and comparisons → Release rerun → next item. A new subsystem gets its plan in `docs/plans/<subsystem>/` first.
- **Evidence:** local captures, the tools and the [reference architecture](../reference/REFERENCE_ARCHITECTURE.md); adapt reference behavior, never copy implementations or decompiled game code. Record confirmed facts, hypotheses and unknowns separately; keep unknown names neutral.
- **Content:** game content in ignored `rom/` or `game_data/`, analysis in `work/` or `research/`, generated code in `game/generated/`; never commit proprietary content.
- **End of a pass:** the gate, [PROG.md](../PROG.md), the handoff and the monthly history, a review of the staged files, commit and push.
