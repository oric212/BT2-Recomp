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

## Reached IOP boot boundary (2026-10-04)

Before unfamiliar subsystem/architecture work, consult this review and its linked sources first. Verify every adapted pattern against BT2; keep game-specific behavior separate.

The pinned [BT3 IOP notes](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/README.md) describe separate R3000 module translation, import providers, loader, IOP memory/scheduling and RPC routing. Treat those as architectural guidance; reported module behavior and permissive fallback returns are not BT2 evidence. The inspected README SHA-256 is `59da0bf2c59bfb017bca594c46274b02066fe38a126848cbfca8d27baadc6414`.

BT2 now requests `rom0:UDNL cdrom0:\IRX\IOPRP300.IMG;1` in command 0x80000003. Next validate that actual local image and collect disc-backed boot evidence using isolated reference settings, following Chronicle's user-local disc workflow. Establish a new IOP boot/service lifetime; neither a BT3 inventory nor BT2's earlier standalone CD snapshot establishes reset completion. General IOP scheduling/recompilation remains a separate implementation decision after this evidence.

### Local image validation and submission evidence

The pinned [BT3 IOPRP tooling](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/ioprp.py) supplied ROMDIR inventory guidance. Our authored validator derives offsets from all directory entries and checks metadata/ISO bounds, rather than locating the first ELF as an offset shortcut. BT2's local image matches the disc and contains 16 ELF payloads (inventory/hashes remain ignored). This establishes identity and header metadata, not successful loading/import resolution.

Five independent disc-backed EE checkpoints establish submission acknowledgement and ordered register clears. A new pending IOP lifetime invalidates old service state. Fresh post-reset IOP evidence is still needed before readiness, loader/import providers, scheduling or service routing can be implemented. No reference game implementation was copied.

### Reached GS setup boundary

The pinned GPU interface was consulted again for the GS boundary (SHA-256 `2e70e2cdaad179218e55cd365b54c2308cf6ce46dafd5d9fe9d820f44cd741aa`). It separates command recording from presentation. BT2's next mapped helper includes GS register access at 0x12001000; [the scoped plan](GS_BRINGUP_PLAN.md) starts with actual register evidence before rendering integration. No renderer implementation copied.

### GS setup and new interrupt boundary

Consulted pinned PCSX2 GS.cpp for CSR reset behavior and Hw.h for register addresses, and pinned Play PS2OS.cpp for GS syscall ABI. Source hashes/provenance are in config/startup_abi.sources.json. Local captures determine returned values; Play's void-style handlers do not establish BT2 BIOS return values. The authored adapter records only exact reached requests with explicit reference readback checkpoints, without copying reset/rendering implementations.

The next stop is INTC_STAT acknowledgement followed by polling and a later CSR field read. An initial working hypothesis incorrectly labeled 0x1000F000 as DMA control; the pinned hardware map resolves it as INTC_STAT (DMAC_CTRL is 0x1000E000). No runtime DMA behavior was added from that hypothesis. Follow GS_INTERRUPT_TIMING_PLAN.md before introducing event sources or synchronization.

### Interrupt ordering and GIF DMA boundary

Pinned [PCSX2 HwWrite.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/HwWrite.cpp) establishes status acknowledgement; [Hw.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Hw.cpp) separates status latching from mask-gated interrupt delivery. [Counters.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Counters.cpp) separates Vblank start, later GS field update, and Vblank end. Pinned PS2SDK kernel.h identifies Vblank-start source 2, hence bit 4. Hashes are in config/startup_abi.sources.json. The authored diagnostic sequence uses validated BT2 samples without copying these implementations or claiming cycle accuracy.

The reached GIF DMA channel read now needs [its separate plan](GIF_DMA_BRINGUP_PLAN.md). Use BT3's recording/presentation separation as architectural guidance; validate actual BT2 transfers before acknowledging or decoding them.

### First GIF request staging and drawing packet

Consulted pinned [Dmac.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Dmac.cpp) for channel programming/enable gating, [Gif.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Gif.cpp) for normal transfers, and [Gif_Unit.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Gif_Unit.h) for tag layout. [GSRegs.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSRegs.h) identifies XYZ2, sprite primitive and the three single-bit control registers. Source hashes/roles are recorded in config/startup_abi.sources.json. No implementations copied.

The first reached BT2 packet includes drawing, so channel staging alone cannot justify transfer completion. The [renderer plan](GS_RENDERER_BRINGUP_PLAN.md) follows BT3's separation of ordered command consumption and presentation, scoped initially to observed BT2 behavior.

### Scoped GS software command consumption

Consulted pinned [GSLocalMemory.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSLocalMemory.h) and [GSTables.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSTables.cpp) for 64 by 32 PSMCT32/PSMZ24 pages, 8 by 8 blocks, pixel bit layout and depth block XOR 0x18. The authored sink expresses addressing through bit placement, preserves unknown VRAM bits, and consumes validated packets transactionally. Public register definitions establish ZBUF PSM field 1 as PSMZ24 and TEST ZTST 1 as ALWAYS; ZMSK is bit 32, so both BT2 clears write depth. Source hashes are in config/startup_abi.sources.json. No implementation copied.

The first DMA synchronization is backed by separate before/poll/after, VU-idle and helper-return captures; the second same-form source packet is captured at the latter return. Completion remains a bounded logical diagnostic, independent of physical DMA/video timing. Main startup next requests a new [post-reset SIF transport lifetime](POST_RESET_SIF_PLAN.md); old channel/service evidence must not be reused automatically.

Pinned [GSDrawingEnvironment.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSDrawingEnvironment.cpp) and [GSDrawingContext.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/GS/GSDrawingContext.cpp) were also consulted: drawing reset establishes SCANMSK and FBA defaults. The reached CSR reset explicitly initializes those scoped sink defaults while preserving unknown VRAM; full GS reset/presentation effects remain incomplete.

### Fresh SIF transport and IOP service lifetime

Consulted pinned PS2SDK [EE sifcmd.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/sifcmd.c) and [IOP sifcmd.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/sifcmd/src/sifcmd.c) for INIT_CMD opt 0/1 and command ownership. Local BT2 before/after captures establish fresh buffer addresses and SET_SREG index 0/value 1. Source hashes/roles are in config/startup_abi.sources.json. No implementation copied.

The separate diagnostic adapter completes transport initialization without executing an IOP boot or validating RPC services. BT3's native IOP module/import/scheduler separation guides the [new service ownership scope](POST_RESET_IOP_SERVICES_PLAN.md). Live IOP service records are evidence to validate against a fresh reply, not a reason to reuse old pointers.

### Post-reset service replies and loader boundary

Pinned public PS2SDK sifrpc.c/sifrpc-common.h establish synchronous bind/call headers, packet freeing, client pointers and semaphore signaling. BT2 captures independently validate a null CD bind, later successful bind, CD initialization, SCMD bind and mode-2 return. The authored epoch-owned adapter executes the generated EE RPC_END callback and keeps prior service state invalid; it does not execute native IOP service instructions or reproduce physical retry counts.

Pinned [loadfile.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/loadfile.c) identifies the reached service 0x80000006 as LOADFILE. BT3's distinction between module mapping, relocation/import resolution, entry execution and scheduling guides [the loader plan](LOADFILE_IOP_MODULE_PLAN.md). Confirm our toolchain's actual capabilities and BT2 requests before implementation. Source hash is recorded in config/startup_abi.sources.json; no implementation copied.

### LOADFILE and actual pinned IOP capability

Inspected pinned BT3 [IRX discovery](https://github.com/z3xox/BT3-Recomp/blob/e22a34beb564e2d0f32bd60b02f0a95d9645ddcc/tools/iop/irx_functions.py), SHA-256 `83465ff5c1a6e167fa5e6fe8ef485784ad09f034c05e026e3114c70f5d41e1c0`, and pinned PS2SDK [irx.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/kernel/include/irx.h), SHA-256 `2afc986bdf65970f599f635ed3c5c73353f8ed6a83f2ca186c285c927bdddff7`, for table layout, ordinal stubs and map seeds. The authored inspector checks bounds/stub encodings and preserves code between inline tables; it does not assume that the first table ends all executable code. No implementation copied.

Audited our pinned PS2Recomp `RecompilerConfig` in `types.h` (SHA-256 `f33027cc08de16084ef27ca1b2593e4736a0734031cd619b62e4981a774041d3`) and `function_emitter.cpp` (`a1e3267a8f81d032a29ab7a2d260b2f501425b6bbcb4d244c3315be6d5e34c34`). No R3000 architecture selector/import dispatcher is present; emission uses the EE context. BT3's added IOP backend therefore needs a [separate native execution scope](IOP_NATIVE_EXECUTION_PLAN.md).

Local SIO2MAN metadata and private relocation preparation are compared against reference loaded-module ownership described by public [loadcore.h](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/loadcore/include/loadcore.h). Matching relocated code does not establish entry execution, library registration or native scheduling.

### Isolated native IOP backend and first library

Reviewed pinned BT3 instruction_translator.cpp, function_emitter.cpp and ps2xRuntime/src/lib/ps2_iop_module.cpp: its R3000 guards/import dispatch and separate module lifetime informed the authored backend. Its context coupling and permissive import defaults were not adopted. The pinned EE dependency remains unchanged.

Public PS2SDK [modload.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/modload/src/modload.c) establishes argc including the module path, argv termination and GP initialization. [loadcore.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/iop/system/loadcore/src/loadcore.c) and irx.h establish export registration/layout and linkage mutation. The authored provider supports only validated first registration in an owned lifetime; no copied implementation or automatic successful imports.

Pinned PCSX2 [IOP writes](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/ps2/Iop/IopHwWrite.cpp), [SioTypes.h](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SIO/SioTypes.h) and [Sio2.cpp](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/SIO/Sio2.cpp) distinguish control reset value 0x3BC from START_TRANSFER bit 0 and IRQ effects. They establish the next hardware scope; neither transfer completion nor always-ready behavior follows from reset alone. Synthetic execution verifies scoped CPU semantics, not physical timing equivalence.

### Reached SIO2MAN kernel startup

Consulted pinned public PS2SDK intrman.h/intrman.c, dmacman.h/dmacman.c, thbase.h/thbase.c, thevent.h/thevent.c, thsemap.h and threadman thcommon.c/include/thcommon.h at ac92a9f657d2e531dd8f060250b07f2a5ac6dea5. Source paths are under iop/system/. Parameter layouts, scoped DMA priority/enable, enabled kernel startup, retained GP/stack and event mask/OR/CLEAR behavior guide authored providers. No source implementation was copied. The observed retail kernel record layout differs from SDK thcommon.h; the verifier labels its scenario-specific offsets and checks object tag/ID, state, module entry/GP and wait ownership before comparison. Opaque options are not interpreted. See [SIO2 scope](SIO2_BRINGUP_PLAN.md) and [persistent EE bridge](IOP_LOADFILE_BRIDGE_PLAN.md).
