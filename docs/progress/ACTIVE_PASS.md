# Resume handoff: IOP hardware timers and the SOUNDS worker

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **585 entered / 576 completed**, kernel 2/2, 92,166 calls / 92,104 returns, 173,374 IOP instructions / 248 targets, 42 CRI calls, 9 heap calls / 2 frees, 15 module loads, 117 logical VBlanks + 2 polled, 17 Timer 2 overflows, 57 original disc reads, three EE→IOP uploads, three sound-driver calls. Boundary: SOUNDS command `0x20` (client `0x6351C0`), validated on the EE side and stopped before the IOP is entered (`sounds_hardware_timers_supported` is false).

## Next implementation

Follow [IOP_HARDWARE_TIMER_PLAN.md](../IOP_HARDWARE_TIMER_PLAN.md). Enabling the command reaches `timrman` ordinal 4 (AllocHardTimer) inside the started SOUNDS worker; the scoped sleep/wakeup and clock-conversion providers already carry it that far. Original evidence: `rpc-sound-bank-after` (after the loader returns). The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest. New comparison: `verify_sound_bank.py`. Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
