# Timed IOP wait and active RPC context scope

Status: new scheduler design boundary; not implemented.

## Reached evidence

The real initial MCSERV query executes six absent-card DMA pairs and stops at thbase ordinal 33 (DelayThread), import PC 0x31B40, argument 300 microseconds. The local MCMAN caller at 0x29E50 sets 300 in the delay slot; retain its neutral name. Thirteen modules are resident, twelve workers wait and a priority-24 sound helper is already ready. MCSERV worker priority is 104; SIO2 worker priority is 16.

An independent isolated retail flow, with both card slots disabled, passes this wait and reaches secondary SIF submission to 0x3A7480, 192 bytes, flags 0. That establishes a later path, not native scheduling equivalence or a replacement payload. Captures/generated modules remain ignored.

## Required design/evidence before a timed provider

- Consult REFERENCE_ARCHITECTURE.md and pinned BT3 cooperative scheduler/IOP ownership interfaces plus public PS2SDK DelayThread/thread contracts. Adapt the architecture; do not copy implementation or reference game behavior.
- Preserve the active borrowed RPC CPU/GP/SP, queue ownership and pending EE request across suspension. executeRegisteredRpc currently owns a local CPU and treats exceptions as a failed service; a timed yield must be distinguished from failure and retain the actual handler continuation.
- Establish owned logical time units and a single deadline for this 300-microsecond request. Capture pre/post wait and actual caller return/context where needed. Do not claim wall-clock or cycle accuracy from a logical clock.
- Dispatch eligible ready workers by established priority while the caller waits. Inspect the existing sound helper's actual execution blocker rather than skipping it or assuming it cannot affect startup.
- Resume the exact suspended handler once its deadline is reached. Preserve interrupt state, stack ownership and nested event/preemption contexts; reject unrelated delays until evidenced.
- Then follow secondary SIF descriptor/poll, copy actual native stack bytes and deliver actual primary RPC response and generated FUN_0031DD60 callback. Discover the callback from execution/capture provenance when needed.

## Verification gates

Use synthetic suspend/resume and priority-order cases with observable different continuations; reject duplicate wake, wrong owner/deadline/epoch and failed-context retry. Confirm the real query's owned stack/queue and pending EE request survive, and only one timer wake occurs. Compare actual final result/transport with an independently captured explicit absent-card profile, without equating unrequested stack contents or physical scheduling.

An immediate-success DelayThread stub is not an implementation of this scope. Keep the pending boundary until context retention and eligible worker dispatch are supported.
