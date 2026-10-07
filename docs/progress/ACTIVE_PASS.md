# Resume handoff: GS textures

Updated: 2026-10-07. **Pass verified and committed; next subsystem planned, not started.**

Normal checkpoint: **642 entered / 633 completed**, kernel 2/2, 97,963 calls / 97,899 returns, 229,661 IOP instructions, 123 logical VBlanks.

The interrupted SPU2/frame VSync checkpoint (`149466c`) is now verified (see the 2026-10 history).

**Boundary.** The second frame's render kick (`0x28B3E8`) starts VIF1 chain DMA at `0x430600`. The chain carries PATH2 DIRECT GS traffic: draw state, a 256×32 CT32 upload of a 512×128 PSMT4 texture, a 16-entry CLUT and one textured, blended sprite. It is refused as a checked boundary at the channel start, with no channel effect (`vif1_blocked_*` report fields).

## Next implementation

Follow [GS_TEXTURE_PLAN.md](../GS_TEXTURE_PLAN.md):
1. VIF1 code parsing with DIRECT to the GS;
2. host→local IMAGE transfers;
3. PSMT4 addressing and the CSM1 CLUT;
4. the textured sprite.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`.

**Phase-matched comparisons.** Set `BT2_PHASE_STOP=<hex>` to stop the probe at the first entry to, or return to, that address, then copy the report before the next run. Phases:
- `311860`: `verify_dbzp_load.py`;
- `310518`: `verify_frame_vsync.py`;
- `28b218`: `verify_overlay_dispenv.py`, `verify_display_reset.py`, `verify_cri_autodma.py`, `verify_dbc_rpc.py`.

At the final checkpoint, these change for legitimate later-frame reasons: overlay data, stack reuse, controller client IDs and frame cache operations.

**Earlier-phase-only verifiers:** `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.

**Still open:** the IOP stream consumer acknowledgement is unmodelled, and the asset bytes have not yet been compared with the original.
