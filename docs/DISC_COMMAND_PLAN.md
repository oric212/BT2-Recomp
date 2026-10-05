# Bounded first disc command and sector read

1. Capture the reached 0x80000595 bind and actual semaphore/return. Validate the disc-matched CDVDFSV handler, buffer and GP; preserve boot-service limitations.
2. Capture the first real command, mode, sector range, destination and callback. Use local pinned BT3 ownership patterns and public CDVD/SIF contracts; adapt to BT2 observations.
3. Execute the original translated command handler with scoped imports. Supply read-only local ISO bytes only for validated ranges and modes, retaining buffer ownership and completion state. Do not substitute a captured asset payload.
4. Verify original secondary transfers and primary completion against isolated retail evidence; follow generated EE callback/cleanup and related command paths.
5. Continue bounded startup requests until a new major subsystem, inaccessible evidence or a speculative next step. Test, update progress/history, commit and push. Keep all proprietary inputs/captures/generated code ignored.

An accepted asynchronous command is not proof of completed sectors. No boot-entry residency, general CDVD hardware, arbitrary timing or rendering is inferred.

## Verified boundary (2026-10-06)

Command-service bind and its semaphore/cleanup complete. The original command-14 readiness handler returns 2 after independently captured DVD type 0x14, status 0x4C, SC(-3) result 0 and SC(-12) result 1. Native primary reply and generated EE cleanup complete. No readiness result substitutes for sector completion.

First sector request is asynchronous command 1 / RPC ID 46: LBA 0xE2A, count 2, 2048-byte sectors, destination 0x39DE80, packed mode 0x100, callback 0x31ECF8 with argument 0x3694C0. Independent retail after-capture contains exactly the 4096 ISO bytes and clears the busy flag. Native stops before accepting this request.

The next major implementation is CDVDMAN shared read-state ownership and completion. Diagnostic original handler execution reaches a null workspace at +0xF4C; CDVDFSV initialization obtains the pointer at +0x2B4 through SC(-14). Captured retail workspace 0x3D380 contains external pointers and live driver state. Its fields, buffers, scheduling and completion transitions need a separate driver-state design; copying that snapshot or zero-filling it would not establish ownership. Existing reference patterns and the captured request remain inputs to that design.
