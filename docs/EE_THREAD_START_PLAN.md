# EE thread start and sound callback scope

## Confirmed boundary

After the first real SDRDRV command, generated FUN_00315148 creates owned dormant EE thread ID 3: entry 0x315380, stack 0x3A5CC0, size 0x1000, GP 0x38BDF0, priority 5. It calls FUN_0032FDA0 at 0x315210 with ID 3 and argument zero. The native probe deliberately stops before that call.

Independent before/after retail captures publish CP0 Status **0x70030C11** and successful return **3** at 0x315218. The native context still defaults Status to zero. FUN_0032F730 tests its interrupt-enable bit, so the earlier exploratory run reported failure and continued down a different error path. Those exploratory downstream call counts are invalid as a boot-progress measure. The authoritative checkpoint is 1,007 calls / 1,003 returns.

SDRDRV also starts an IOP helper at module+0xFF0, priority 24, stack 0x800. It is ready natively and running in retail. Its generated path includes an outbound bind to service 0x80000704. Registration, invocation and completion of the corresponding EE callback are still unknown.

## Required design and evidence

- Consult REFERENCE_ARCHITECTURE.md and pinned public EE kernel/thread and SIFRPC contracts. Establish ownership of CP0 Status, interrupt transitions, saved EE contexts and thread state without copying retail kernel records or reference implementations.
- Trace generated StartThread through its status/thread query and actual syscall; compare the existing CreateThread record with independent retail descriptor/context captures. Validate changes before exposing ready state. Do not merely return success or set the full retail Status word as a universal default.
- Define scheduling at the actual blocking/yield boundary with main priority 2 and the new priority-5 thread. Preserve registers, stack, GP, arguments and return state. Reject unsupported thread operations explicitly.
- Trace the EE thread to its first wait/RPC registration and the IOP helper to its outbound request. Resolve this bounded callback pair together; determine ownership and required transport/event behavior before dispatch.
- Rerun toward the independently captured SOUNDS bind to 0x02000004, client 0x6351C0, RPC ID 29, caller return 0x297608. Existing native registration is available, but its bind/request delivery has not been retained as completed work while the earlier EE transition remains invalid.

## Forward evidence and limits

A local DECI2 subfunction-16 checkpoint returns all ones and contains a disc-file diagnostic. Authored console decoding and generated formatter ABI support have synthetic tests, but the valid native path has not reached that message. A diagnostic saying a file was found does not prove asset reads or loading.

No runnable EE scheduler, IOP-to-EE callback completion, disc-file read, asset loading, audio playback or displayed frame is claimed. This is a new execution/scheduling scope requiring its own design before changing startup state.
