# Resume handoff: memory card check

Updated: 2026-10-08. **Pass verified and committed; next blocker identified, not started.**

Normal checkpoint: **682 entered / 672 completed**, kernel 2/2. The run covers 27 frames: 133,549 calls / 133,468 returns, 1,096,021 IOP instructions, 150 logical VBlanks.

**What runs now.**
- **IOP VBlank:** start and end edges are delivered every logical field, as the public vblank module signals its event flag.
- **Pad:** DS2U_D configures the emulated DualShock 2 on port 1 (analog, then pressure mode), and SIO2D rescans ports.
- **DBCMAN:** pad state is copied to the game's registered socket buffers.
- **CRI:** stream buffers are returned to the EE.
- **Game:** with a controller present it leaves the controller-missing screen. After DBCMAN socket and device requests it starts a memory card check.

**Boundary.** At frame 27 the game sends a no-wait MCSERV query (client `0x3A6F80`, function 1, 48 bytes in, 4 out, end callback `0x31DD60`). Card queries are only admitted in the captured startup profile, so the native IOP card machinery needs generalizing: MCSERV, MCMAN, the absent-card SIO2 probes and the card alarm.

## Next implementation

1. **Memory card check.** Generalize the frame-phase MCSERV queries through the original handlers with absent cards, reusing the verified absent-card SIO2/alarm path. Capture the original at the same point for comparison. Expect a "no memory card" notice or the next screen.
2. **Optimized build.** The live window runs at about 12 fps in Debug, so real-time speed needs an optimized probe build.
3. **Host input.** Feed the emulated pad from host controllers (XInput / SDL) once screens wait for a button press.

## Verification and evidence

Reports are the ignored `work/bt2-generation/entry-stop.json` (+ `.ee.bin`, `.iop.bin`, `.gs.bin`).
- **Viewer:** `run_startup_probe.py --viewer` shows frames live.
- **Watchpoint:** `BT2_IOP_WATCH=<hex>` stops on the first nonzero IOP store to an address.
- **Note:** CTest overwrites `entry-stop.json`, so rerun the normal probe afterwards.

**Phase-matched comparisons.** Set `BT2_PHASE_STOP=<hex>` and copy the report before the next run.
- `311860`: `verify_dbzp_load.py`;
- `310518`: `verify_frame_vsync.py`;
- `28b218`: `verify_overlay_dispenv.py`, `verify_display_reset.py`, `verify_cri_autodma.py`, `verify_dbc_rpc.py`, `verify_mc_rpc.py`, `verify_sound_bank.py`.

**Earlier-phase-only verifiers:** `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
