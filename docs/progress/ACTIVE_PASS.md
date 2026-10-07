# Active pass: SPU2 playback and frame VSync — unverified

Updated: 2026-10-07. **Committed at the user's request with final testing pending. The verified checkpoint remains 620 / 609 at starting commit `4454ba1`.** Do not promote the current implementation or diagnostic counts into verified progress.

## Implemented, unverified

- `runtime/iop/spu2_playback.h`: owned ADPCM block flags, 28-sample blocks, pitch/position, KON/KOFF and ENDX; no samples, ADSR or mixing. `native_runtime.h` splits logical fields at each SOUNDS timer compare; `module_service.h` enables the bounded model. Active pitch/manual-loop changes and ENVX stay unsupported.
- `runtime/ee_sound_dispatch.cpp` and `runtime/probe_abi/ps2_runtime.h`: admit reached sound timer ticks, refresh IOP report counters, register one-shot SetVSyncFlag pointers and publish the flag/CSR through the existing VBlank handler/worker path. Original registration preserves V0. Physical timing and FIELD phase are excluded.
- `runtime/entry_probe.cpp`: private playback/frame counters; preserve a report when the existing execution budget is exhausted, without treating arbitrary runtime errors as checked stops.
- Synthetic fixtures in `runtime/iop/spu2_playback_tests.h`, `native_tests.cpp` and `bootstrap_tests.cpp`; capture points and comparisons in `tools/capture_ee/`. The newest fixtures and final combined state have not passed the full verification gate.
- Ignored mapped selections: executable 0x32F180, 0x32EFD0, 0x28B218, 0x100D58; DBZP overlay 0x3BC5A8 and 0x3BC460. Prepared/generated code remains local.

## Evidence and latest boundary

Pinned PCSX2 SPU2 register/KON references and Mixer.cpp establish halfword addresses, flags and pitch. See `docs/SPU2_VOICE_PLAN.md` and `docs/REFERENCE_ARCHITECTURE.md`; public references are ignored local copies. Pinned Play!/PS2SDK guidance was adapted for VSync, with the actual return behavior determined by captures.

Original captures `rpc-frame-vsync-before`, `rpc-vsync-flags-after`, `rpc-frame-vsync-after` and `rpc-frame-end` succeeded. The first wait publishes flag 1 and CSR 0x551B400C. Completed frame bookkeeping sets both field-buffer words to 1; the phase-matched display block and entire overlay matched an intermediate native checkpoint.

Normal exploratory startup advances eight SOUNDS ticks, completes 400 Timer 1 polls and the first frame wait/cleanup, then enters the indexed-asset loader. The latest ignored normal report (`work/bt2-generation/entry-stop.json`) records **626 entered / 616 completed**, 210,874 IOP instructions, 119 logical VBlanks and `execution_budget_exhausted=1` at EE PC 0x3BC4C8. Its 3,609,471 calls / 3,609,411 returns are inflated by the stalled poll and are not a new verified checkpoint. Do not mix these with the separate sound-worker diagnostic.

New original capture `rpc-asset-poll` at 0x100E10 succeeded. Its loader slots match the native poll (0x366A58, 0x12, 0x8CC200); loader state differs (original 1, native 2). The entire overlay and slot pointer 0x8CC140 match this phase. No asset-poll scheduling fix has been implemented. Existing logical time advances at owned idle/frame boundaries; this busy poll has exhausted the budget without further VBlanks. Trace request ownership and wake/time requirements before changing scheduling.

## Testing remains unverified

Earlier isolated runs passed playback/MMIO fixtures and explicit entry builds; Python suites passed 175 capture, 63 recompilation, 13 IOP and 3 Ghidra tests before the latest capture/report changes. TEMP/TMP under ignored `work/test-temp` avoided Windows sandbox temporary-directory errors. These are partial evidence, not final pass validation.

An intermediate normal checkpoint passed 25 capture comparisons. Later loader execution changes stack publications and overlay data, so older-phase DBZP/overlay/frame comparisons need phase-matched inputs or retained earlier checkpoints. `work/bt2-generation/verify-spu2-pass.py` and its logs are exploratory; do not report its latest failures as resolved. The separate sound-worker diagnostic has not been rerun.

Remaining: inspect/review the newest capture and budget reporting, build ALL and explicitly rebuild `bt2-entry-probe`, run 17 CTests and relevant Python suites, rerun normal and separate diagnostic probes, compare applicable original phases, then publish only allowlisted verified counters and synchronize progress/README/dashboard. No new verified export was made in this interrupted pass.

The user explicitly requested stopping development/testing and committing/pushing with testing left unverified. No Astra High agents were used for the remaining work. Five-hour usage was last checked at 58% used / 42% remaining.
