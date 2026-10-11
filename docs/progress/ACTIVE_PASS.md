# Resume handoff

Updated: 2026-10-11. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames). The run stops at a read of GIF DMA channel 2 TADR (`0x1000A030`, pc `0x312084`). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Replace emulated PS2 hardware with a minimal native layer under the recompiled game code (decided 2026-10-11): native RPC services instead of the emulated IOP, and GPU rendering through paraLLEl-GS. Follow "Current next step" in [the implementation plan](../project/IMPLEMENTATION_PLAN.md#current-next-step). Every replacement keeps the game running to the current checkpoint with game-visible results unchanged, proven by tests and PCSX2 comparisons.

### 1. Native services plan

Write `docs/plans/iop/NATIVE_SERVICES_PLAN.md`. For each service the game binds, record the requests it sends at the current checkpoint and the EE-visible results each must produce:

| Service | Reached use (2026-10-08 report) |
| --- | --- |
| Disc commands (NCMD/SCMD) | 186 sector reads, 6 disc RPC calls, 185 SCMD calls |
| FILEIO and IOP heap | 6 file calls (`DBZP.BIN` open/seek/read/close), 9 heap calls |
| Controller (DBCMAN) | 4 binds, 4 calls, then per-field port requests |
| Memory card (MCSERV) | 1 bind, 2 calls, 77 queries (absent card) |
| SOUNDS / sound driver (SDRDRV) | 6 and 3 calls; sound block and bank uploads |
| CRI ADX (`CRI_ADXI`) | 43 calls, 2,070 stream transfers |

Sources: existing reports, `runtime/iop/services/` and `runtime/iop/ee_bridge/` (the current request validation), the per-service plans in `docs/plans/`, and [Tenkaichi3Decomp](../reference/REFERENCE_ARCHITECTURE.md#tenkaichi3decomp-native-port-reference) (`plat_sys.c`, `plat_stub.c`, `plat_mc.c`, `snd_se.c`, `snd_adx.c`). The plan also lists the replacement order and which comparisons stay valid; comparisons of IOP internals are retired with the code they checked.

### 2. Replace services one at a time

Disc and file reads from the ISO first, then the controller (host input, keeping the pad script for tests), the memory card (host files), then SOUNDS, the sound driver and CRI ADX (native ADX decoding and sample playback). After each one: the game reaches the same checkpoint, the EE-visible comparisons pass, a unit test covers each request type, and the IOP code it made obsolete is removed.

### 3. GPU rendering

Write `docs/plans/graphics/GPU_RENDERING_PLAN.md`: build paraLLEl-GS (snapshot `cc6184af`, Vulkan, Granite) with MSVC, feed it the existing GIF/VIF1 output (`gif_transfer`) and the privileged display registers, and present its `vsync` scanout in the live window. Match it against the CPU renderer on the 650 startup frames before switching.

### 4. GIF DMA (current blocker)

- **Evidence first.** The routine at `0x312060` reads TADR (`lw` at `0x312084`), rewrites it unless it reads `0xFFFFFFFF`, then reads CHCR, clears its MOD bits (mask `0xFFFFFFF3`) and zeroes QWC. Follow it to its channel start and record the final CHCR, TADR, MADR/QWC and, if chained, the tag chain.
- **One GIF model.** GIF channel 2 has four models today: `gif_dma` and `clear_repeat_gif` (`GifDmaStaging`), `texture_gif` (`TextureGifStartup`) and `frame_gif`. Consolidate them, then add chain mode by sharing the VIF1 chain walker in `frameVif1Start` (refe/cnt/next/ref/refs/end, tag transfer; call/ret and tag IRQs refused).
- **Tests:** a unit test per reached tag type, register checks after each transfer, VIF1 tests passing through the shared walker, and a comparison with an original capture.

### 5. Opening movie

The movie is CRI Sofdec (`ZS2USOP.SFD`). Write `docs/plans/video/SOFDEC_PLAN.md` (native video decoding plus ADX audio, no IPU emulation) before implementing it.

## Pass completion

1. **Debug gate:** ALL and `bt2-entry-probe` build, all CTests and the Python suites pass, and the sound-worker diagnostic is unchanged (or retired with the code it checks, with the reason recorded).
2. **Original comparisons:** currently **42 of 46 pass**: 23 on the full run, 12 at the phase stops (`BT2_PHASE_STOP` = `311860`, `310518`, `28b218`, `24b81c`) and 7 with their own inputs. Known failures: `cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization`. No new failures, except comparisons retired with replaced IOP code, each recorded with its reason.
3. **Release rerun:** same checkpoint as Debug, with a frame render checked visually.
4. **Docs:** PROG.md, the monthly history, this handoff with the next actionable step, `components.json` if a status changed, `sync_progress.py --check`, links resolving, no mojibake.
5. **Commit and push**, with no attribution lines.

The full gate is a scratch script, not a tracked tool; its steps are this list.

### Comparisons outside the gate script

Run these with their own inputs (`$C=work/pcsx2-capture`, outputs to a scratch folder): `verify_cd_init.py $C/cd-init-after/capture.json --semaphores-capture $C/cd-semaphores/capture.json`; `verify_rpc_bind.py $C/sif-bind-after/capture.json --call-sema-capture $C/rpc-call-sema/capture.json`; `verify_heap_bind.py`, `verify_heap_allocation.py` and `verify_heap_copy.py` with `--capture $C/fileio-bind-after|heap-alloc-after|heap-copy-after --disc <ISO> --output <scratch>`; `verify_spu2_transfer.py --disc <ISO>`; `verify_disc_read.py --disc <ISO> --native work/bt2-generation/sound-worker-stop.json --workspace 0x1D4B00`. `verify_rpc_bind.py` and `verify_cd_init.py` rewrite their probe inputs in `work/bt2-generation/`; back them up first and confirm they are unchanged.

## Deferred follow-ups (not started)

- **After the movie:** the title screen waits for Start; without input the game runs a CPU-vs-CPU demo fight (user report).
- **Frame field:** handler-delivered VBlanks while main sleeps do not toggle `display_field`. Check against an original capture if a field-dependent path diverges.
- **Host input (XInput/SDL, keyboard), 16:9:** unchanged.
- **Map-gap iterations** rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` could be split by subsystem.
- **Loop and gate tooling:** the selection-loop driver, receipt refresh and full gate still live in scratch scripts; move them into `tools/`.
