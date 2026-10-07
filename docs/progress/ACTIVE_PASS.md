# Resume handoff: IOP SPU2 voice transfers

Updated: 2026-10-07. **Pass verified and committed; next subsystem not started.**

Normal checkpoint: **584 entered / 576 completed**, kernel 2/2, 92,090 calls / 92,028 returns, 172,691 IOP instructions / 248 targets, 42 CRI calls, 7 heap calls / 2 frees, 15 module loads, 117 logical VBlanks + 2 polled, 17 Timer 2 overflows, 57 original disc reads, one EE→IOP sound-block upload. Boundary: main's sound-driver command `0x80D0` (client `0x3A5C80`, service `0x80000701`), validated on the EE side and stopped before the IOP is entered (`sound_voice_transfers_supported` is false).

## Next implementation

Follow [IOP_SPU2_TRANSFER_PLAN.md](../IOP_SPU2_TRANSFER_PLAN.md): a general owned SPU2 DMA write (TSA/ATTR/DMA4 registers, copy into `spu2_sound_ram`, logical completion) and IRQ 36 delivery through LIBSD's handler `0x927E8` before the issuing thread waits. `NativeIopService::callSoundCommand` already executes later commands through the original registered handler once enabled; without the SPU2 model it stops at LIBSD's DMA4 status read (`0x92250`). The IOP stream consumer acknowledgement remains unmodelled. `div.s` is still emitted inline with IEEE semantics.

## Verification and evidence

Reports: ignored `work/bt2-generation/entry-stop.json` and `sound-worker-stop.json`. Run the normal probe after CTest. Earlier-phase-only verifiers: `verify_disc_read.py`, `verify_spu2_initialization.py`, `verify_cri_rpc.py`, `verify_cri_preemption.py`, `verify_second_heap.py`.
