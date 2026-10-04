# File I/O startup boundary

## Observed entry

Following thirteen native IOP loads, generated FUN_00336CD8 calls the existing RPC bind helper with server ID 0x80000003 and client 0x3AC840. Execution currently stops at CreateSema (selector 0x40, PC 0x32EC68), with max 1, initial 0 and option 0x3838D8. The reached descriptor includes stack residue in its attributes. Existing CD/loadfile-specific exceptions do not authorize this new client.

This is a new subsystem boundary: native module-load replies do not establish file I/O service binding or file reads. Do not send this bind through the module-load handler or synthesize success.

## Bounded next pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned SDK file I/O/RPC sources for public protocol and service ownership. Derive BT2 request details from its generated execution and local captures.
2. Locate the actual server in the local post-reset IOP image/state and capture the unique EE bind return, descriptor and reply. Establish its worker/module lifetime before defining the response.
3. Extend the checked semaphore path only for this verified client. Route the actual bind through owned service state and run to the first real file request.
4. Implement the bounded reached operation against user-local disc data with checked paths, handles, offsets, lengths and failures. Preserve existing driver/IOP ownership; no host filesystem exposure or permissive fallback.
5. Continue through related file I/O blockers toward asset loading. Stop for a new major subsystem, unavailable evidence/access or speculative changes. Test, update PROG/history, commit and push at pass end.

Inputs, captures, protocol dumps and proprietary-derived generated code remain ignored. Unknown functions retain neutral names. No file I/O implementation is claimed by this plan.
