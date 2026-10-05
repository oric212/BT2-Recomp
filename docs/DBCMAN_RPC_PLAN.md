# DBCMAN EE request scope

## Reached boundary

After valid EE sound thread start, owned SOUNDS bind and native command 0x10, FUN_0028BCF8 reaches FUN_003131B8. Its first bind requests service **0x80001300**, client **0x3A03C0**, RPC ID **31**. The current checked stop is CreateSema 0x40 at 0x32EC68, descriptor at initial SP minus 400. It asks max 1 / init 0 / option 0x3838D8; attr contains prior stack data 0x203A97C0, consistent with the synchronous RPC helper's unused attr field. No DBCMAN client request completes natively.

Native DBCMAN registers four owned services: 0x80001300, 0x8000131C, 0x8000131E and 0x8000131F. First owned descriptor 0x62DE0, buffer 0x62F08, handler 0x61C88, queue 0x62D80 and GP 0x6A720 belong to its existing parked priority-20 worker. The new probe inventory reports registrations without asserting command support.

Independent retail first-bind return at **0x313254** publishes return 0 / SP 0x1FFEC40. Client [0,31,7,0,0,0x7AB38,0,0,0,0x7AA10] resolves the actual retail registered server. These captures stay ignored. FUN_003131B8 also binds 0x8000131C at return 0x3132F4; its later paths still need evidence.

## Bounded next pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned public SIFRPC/module contracts. Trace this actual local module's command dispatch and callbacks without guessing DBCMAN's purpose or copying another game's implementation.
2. Capture/validate the related bind and first command group. Adapt the owned registration mapping, generated RPC_END and semaphore cleanup together, rather than ending after the first bind.
3. Deliver only reached command shapes to actual translated handlers on their retained GP/stack. Record cross-space destination ownership, callbacks, allocations and worker waits. Resolve the bounded initialization/request group using local before/after evidence.
4. Establish any requested hardware/service, EE callback or scheduling transition before supplying a result. Resident entry and initial SIO2 startup polls do not validate DBCMAN requests or later device traffic.
5. Continue through the group toward the next real loading/rendering blocker. Keep the existing sound-ready state and waiters intact. Reject stale epochs, unknown services and unvalidated replies.

This is a new EE/IOP command-protocol scope; the existing SIO2 startup plan does not implement these requests. No device capability, controller input, file/asset loading, sound playback or physical timing is inferred from the service IDs.
