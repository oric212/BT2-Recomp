# Resume handoff: live window, then controller input

Updated: 2026-10-08. **Pass verified and committed; next work planned, not started.**

Normal checkpoint: **642 entered / 633 completed**, kernel 2/2. The run ends at the checked 120-frame budget with 225,038 calls / 224,871 returns, 1,181,339 IOP instructions and 357 logical VBlanks.

**What the frames show.** Each frame from the second on uploads a PSMT4 texture and CLUT and draws one textured sprite. The displayed framebuffer reads "No DUALSHOCK®2 analog controller connected. Verify that a DUALSHOCK®2 analog controller is properly inserted into controller port 1."
- Render it with `python tools/capture_ee/render_gs_frame.py --native work/bt2-generation/entry-stop.json --output work/frames/frame-latest.png`.
- The game repeats this screen while both controller-port requests report no device.

## Next work

1. **Live presentation window** (agreed with the user). A host window presents the DISPFB framebuffer at each flip, and a viewer run mode keeps showing frames until a stop, with VBlanks paced to about 60 Hz. Write a short plan first.
2. **Controller input.** Follow [CONTROLLER_PLAN.md](../CONTROLLER_PLAN.md): trace the DBC/DS2U_D/SIO2 driver path, then model a neutral DualShock 2 on port 1.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` (+ `.ee.bin`, `.iop.bin`, `.gs.bin`) and `sound-worker-stop.json`.

**Phase-matched comparisons.** Set `BT2_PHASE_STOP=<hex>` and copy the report before the next run.
- `311860`: `verify_dbzp_load.py`;
- `310518`: `verify_frame_vsync.py`;
- `28b218`: `verify_overlay_dispenv.py`, `verify_display_reset.py`, `verify_cri_autodma.py`, `verify_dbc_rpc.py`.

**Earlier-phase-only verifiers:** `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.

**Still open.** The IOP stream consumer acknowledgement is unmodelled. Asset bytes and the drawn framebuffer have not been compared with an original GS dump, and bilinear rounding is unverified.
