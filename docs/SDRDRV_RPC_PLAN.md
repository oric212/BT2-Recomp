# SDRDRV EE RPC delivery

## Reached boundary

After the second heap allocation and generated FUN_0028BC08 helper, FUN_00314C68 reaches SIF submission selector 0x77 at EE PC 0x32F028. Current checkpoint: 983 calls / 975 returns. The generated request binds service **0x80000701**, client **0x3A5C80**, RPC ID **27**. Descriptor is [0x203A97C0, 0x1E640, 64, 0x44], with SP initial minus 464. No sound bind reply or request completes yet.

Actual translated SDRDRV worker registration constructs this service ID, descriptor at module+0x15E0, handler at module+0x588 and buffer at module+0x1C30. Its existing queue/worker are owned and waiting. Registration does not mean the handler has executed. A bind followed by FUN_00315148 with argument 5 is visible in the generated caller; the request payload and handler effects still require tracing.

This is a new sound RPC delivery scope: existing native RPC loops explicitly reject pending requests. Heap-handler calls do not establish correct queue ownership, worker-context execution, cross-space copies or sound command completion.

## Next implementation pass

1. Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP runtime routing and public SIFRPC contracts. Adapt the owned server/queue/worker pattern without copying permissive fallbacks.
2. Capture the unique generated bind return at 0x314CA4 and next request/return as necessary. Validate original ELF/disc/environment, actual client/server/buffer, publication, request and result. Normalize translated module addresses; do not replay retail addresses as native ownership.
3. Define the bounded fresh-epoch EE/native server pointer mapping, synchronous request copying and retained worker context. Reject unknown/stale servers, malformed extents, callbacks and unsupported commands before EE acknowledgement.
4. Deliver the reached command to real translated SDRDRV code with its registered GP/stack; preserve queue lifetime and generated RPC_END/semaphore cleanup. Trace required library exports and sound effects together. Do not replace an unsupported sound operation with success.
5. Compare captured results and owned effects, run through related blockers, then test/update progress/commit/push. Audio playback, scheduling/timing and general sound command support remain separate claims.
