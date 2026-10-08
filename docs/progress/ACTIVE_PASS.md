# Resume handoff: unverified pass after the memory card check

Updated: 2026-10-08. **Committed at the user's request with final testing pending.** The verified checkpoint remains **682 entered / 672 completed** at commit `66d9745`. Do not promote the numbers below into verified progress until the gate passes.

## Implemented, unverified

- **Frame-phase memory card queries** (`compiled_modules.cpp`, `ee_loadfile_bridge.cpp`, `native_runtime.h`, `kernel_provider.h`).
  - **Request:** later no-wait GetInfo queries (port 0, slot 0, type/free/format requested) run through the original MCSERV/MCMAN handlers.
  - **Card path:** counters are checked relative to the start of each query (`beginCardQuery`), since each absent-card detection repeats the PS2 then PS1 probe sequence.
  - **RPC worker:** the borrowed worker's wait fields are restored after a registered RPC returns.
  - **Pending alarm:** a card timeout alarm left pending fires at the next field, as logical IOP time now advances one field per VBlank.
  - **SIO2 restart:** a `ctrl|0xC` restart write over a consumed 3-byte reply is accepted cumulatively.
  - **EE side:** frame queries are counted separately (`mc_frame_queries`), outside the startup service accounting.
- **Original comparison.** New capture point `rpc-mc-frame-callback`, taken at the first frame-phase MC end callback (absent cards): reply −11, type 0, free 0, end parameter `0x3A7480`. `verify_mc_frame_query.py` passed against an intermediate native report. `completed_card_query` now accepts repeated queries (counters scale per query; logical time is 1.9 ms per query plus whole fields).
- **SPU2** (`spu2_playback.h`, `native_runtime.h`).
  - **Envelopes:** ADSR from the public ADSR1/ADSR2 layout, PCSX2-style per-sample rate counter. ENVX reads return the level, a finished envelope stops the voice, and an ADPCM end zeroes it.
  - **Voice registers:** volume, pitch, ADSR, SSA and LSA writes take any value. Pitch, ADSR and LSA apply to a playing voice, and an LSA written after key-on overrides the block loop flags. LSA reads return the live loop address. Routing masks take any voice bits.
  - **Key-on:** requires only the keyed voices' volumes to be owned.
- **IOP kernel.** `GetSystemTime` (thbase 34) and `SysClock2USec` (thbase 40), both from logical time. SB/SH stores now pass only the stored byte or halfword to hardware handlers.
- **GS** (`gs_command_sink.h`).
  - **Split:** drawing is split into a validated draw state (`prepare`) and a per-pixel `plot`.
  - **Triangles:** triangles, strips and fans with a top-left fill rule, flat or Gouraud color, affine FST UVs and Z. Report counter `gs_frame_triangles`.
- **Probe.** The EE runaway budget now applies per presented frame (each flip restarts it); run length is bounded by the frame limit.
- **Map tooling** (`prepare_map.py`, unit-tested).
  - **Registers:** the first switch layout decodes its registers (the index/condition swap seen at 0x26DD30).
  - **Delay slot:** stores are accepted in a BEQ guard delay slot (0x2A1AB4).
  - **Result:** main map 9,470 entries (89 quarantined).

## Exploratory result (not verified)

The last normal run before the final selection reached **771 entered / 762 completed** over **105 frames**:
- 328,838 calls / 328,726 returns, 1,992,980 IOP instructions, 228 logical VBlanks;
- 156 GS triangles, no IOP error, one frame-phase card query.

The game passes the memory card check and draws textured triangle strips. The selection loop then selected the next mapped function, `FUN_0026CD98`, and the final ALL/entry build includes it. That build has not been run.

## Remaining verification

**Gate run, 2026-10-08, with the build-time refactor, whose runtime output was shown byte-identical by A/B.**

Passed:
- the ALL/entry build;
- **17/17 CTests**;
- the Python suites: 179 capture, 67 recompilation, 13 IOP and 3 Ghidra tests;
- the sound-worker diagnostic, unchanged at 302/295;
- the normal probe (**772 entered / 763 completed**, 106 frames, boundary `FUN_0026F888`);
- **25 normal comparisons**, including the new `verify_mc_frame_query.py` and `verify_dbc_dma.py`;
- all eight phase-matched comparisons;
- the PNG render (512×448, no unwritten pixels).

**One failure blocks promotion:** `verify_mc_sio2_flow.py` still requires single-query values (`sio2_card_transfer_counts == [6,6]` and the first query's final DMA registers). It needs the same per-query generalization as `completed_card_query`; then export counters (`sync_progress`) and update the dashboard. Inspect the rendered frame visually as well.

## Next implementation

1. **Selection loop:** continue from `FUN_0026CD98`.
2. **Scripted pad input:** without a memory card, the game shows a notice that waits for a button press (user note). Add a deterministic frame-indexed button script to the DualShock 2 model (for example `BT2_PAD_SCRIPT`), and confirm the screen with a PNG first.
3. **Host input:** XInput/SDL and keyboard for the live window.
4. **Optimized build:** for real-time viewer speed.
5. **Later (requested):** save import from emulator card images, real-console exports and internet formats ([memory card plan](../MEMORY_CARD_PLAN.md)); 16:9.
