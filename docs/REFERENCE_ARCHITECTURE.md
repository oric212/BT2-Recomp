# Reference architecture review

Inspected on 2026-10-03, without building either reference or downloading game binaries/assets. BT3-Recomp is the primary architectural reference; Chronicle supplies selected validation practices. No BT3/Dark Cloud addresses, patches, or game implementations are used in BT2.

## BT3-Recomp

Snapshot: `e22a34beb564e2d0f32bd60b02f0a95d9645ddcc`. These are source observations, not independent verification of gameplay claims.

| Area | Observed approach | BT2 decision |
| --- | --- | --- |
| Function maps | Main and overlay CSVs use Name/Start/End/Size and exclusive ends. An oversized-map splitter deals with overlapping analyzer blobs and compiler limits. | Automate provisional neutral EE rows, quarantine noncontiguous/overlapping/oversized bodies, report exclusions. Do not copy BT3 boundaries or guess chunk cuts. [Map preparation](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/split_functions.py). |
| Configuration | Local ELF/map/output paths; per-file generation; syscall instruction handling; address-qualified game stubs and selective COP0/cache patches. | Local configuration and output; no BT3 stub list or instruction patches. [Config](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/config.toml.in). |
| Pipeline | Validate input, prepare map, build recompiler, generate, apply authored patches, install generated sources/headers into runtime, build runner. | One repeatable map/generate/build/run/diagnose command, hashed output provenance, separate host and optional generated probe. [Setup](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/setup.py). |
| Overlays | Separately wraps local gameplay binary as ELF, generates separate maps/tables and adds known re-entry registrations. | Treat BT2's unmapped call as a future execution diagnostic. Do not assume BT2 uses BT3's overlay format or base. [Overlay tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/gen_overlay.py). |
| Runtime/EE | Generated ABI is memory/context/runtime; context has 128-bit GPRs and VU state, address registration and branch dispatch. | Start with a clearly scoped authored ABI adapter for one generated routine; next implement missing entry ABI surface from compiler failures. [Runtime API](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/ps2xRuntime/include/ps2_runtime.h). |
| VU | Manifest identifies local microprogram segments/hashes; setup extracts and translates them. Runtime retains an interpreter fallback. | Keep VU discovery separate from the EE `.text` map; defer until execution needs it. [VU tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/vu1_programs.py). |
| GS/rendering | GPU renderer records commands on GIF/DMA side and replays on the presentation thread. Native engine-seam rendering is a separate evolving path. | Rendering is a later compatibility boundary; no graphics dependency needed for the first generated execution. [GPU interface](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/ps2xRuntime/include/runtime/ps2_gs_gpu_renderer.h), [native seam design](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/docs/NATIVE-RENDER-SEAM.md). |
| Patches/build | Authored post-generation patches use anchors and report missing matches; Windows setup delegates to Python and selects ClangCL. | Keep any future BT2 patch deterministic, evidence-based and checked. Retain our MSVC/C++20 target. [Patches](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/games/bt3/apply_patches.py), [Windows workflow](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/scripts/build-windows.ps1). |

## Chronicle

Snapshot: `05fd37be122cd5833cfeeceb83475933959d938f`. This is a matching decompilation and port, rather than our PS2Recomp pipeline. Region configs associate hashes, symbol maps, and section layout; its validation tools compare built/reference images. Borrow explicit identity, module/symbol provenance and reproducible comparison, without importing its symbols or assuming byte-matching requirements for native recompilation. [NTSC config](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/config/ntsc/main.yaml), [validator](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/scripts/build/verify.py).

Its PCSX2 launcher boots a user's local ISO on the host. Use that separation for future behavioral comparisons with recorded version, input hash, scenario and observations. Its PC platform separation is useful, but the documented Linux/C++26/Vulkan setup is not our Windows/MSVC target. [PCSX2 launcher](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/scripts/host/pcsx2.sh), [PC notes](https://github.com/TheMoonPeople/Chronicle/blob/05fd37be122cd5833cfeeceb83475933959d938f/docs/PC.md).

## Working strategy

Discover provisionally, generate, compile/link, execute, inspect the actual blocker, implement a narrow runtime fix or checked patch, and repeat. Manual reverse engineering follows failing paths. Existing entry/control audits validate tooling; exhaustive manual boundary approval is not a prerequisite for experimental local generation.
