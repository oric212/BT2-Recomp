# Memory-backed IOP module startup

## Observed boundary

After heap allocation/copy, FUN_00320580 calls FUN_003379E0 then FUN_003373F0 with the copied IRX at 0xCE500. The generated request selects LOADFILE operation 6. Current stop is CreateSema at 0x32EC68, parameters 0x1FFEB70, max 1, initial 0, option 0x3838E8. Existing disc-path module replies do not establish loading from a heap buffer.

The complete 864-byte buffer matches the prepared local ELF data at 0x369F90 and an independent retail transfer capture. The native heap owns its destination and retains those bytes. No embedded module identity, relocation, imported services, entry result or residency is inferred merely from the ELF magic.

## Bounded next pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned SDK LOADFILE operation-6/module-buffer contracts. Preserve separation between boot service protocol and translated module execution.
2. Inspect the actual owned 864-byte IRX with existing bounded tools; validate its provenance against the supplied EE executable. Keep extracted bytes/maps/generated code ignored and names neutral until established.
3. Capture the unique generated module-buffer call return, actual loader request, loaded code/ownership and entry effects. Confirm result/ID and required imports before accepting the new semaphore/RPC route.
4. Extend local generation to this validated embedded origin and execute its relocated native entry with checked import providers. Keep pointer relocation and allocator ownership coherent; do not pretend it is a disc path or replay an invented resident result.
5. Compare loaded code/startup effects and acknowledge only scoped validated residency. Rerun through later generated startup toward asset loading/draw submission.
6. Stop only for a new major subsystem, unavailable evidence/access or speculative changes. Build/test, update PROG/history, review authored changes, commit and push.

General LOADFILE buffer support, arbitrary IRX/overlays and file reads are not implemented by this plan. Proprietary-derived output remains ignored.
