# CRI nested IOP thread-start boundary

## Observed boundary

Original CRI command 0x40C accepts twelve bytes `[0xBB80, 0xFFFF, 0xFFFF]` and returns twelve bytes in retail. The subsequent captured 0x422 request observes its result `[0, 1, 0]`. Native original-handler execution reaches thbase ordinal 6 (StartThread) at import PC 0x1745F0. Its borrowed CRI RPC worker has priority 48. The created child is ID 0x2000E, entry 0x16D890 (CRI_ADXI +0x5890), GP 0x17DDF0, stack 0x1E6E00, size 0x800, priority 39. Unknown child purpose remains unassigned.

The runtime rejects higher-priority nested StartThread before initializing or dispatching this child. The original handler has already performed real work; the failed service cannot be retried. No 0x40C response, generated RPC_END completion or native audio is substituted. The existing MCSERV/SIO2 event-preemption path and timed-wait scope do not establish this new child-start/CRI worker lifecycle.

## Design/evidence needed

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 scheduler/worker ownership patterns and public PS2SDK StartThread semantics. Adapt the existing retained-parent approach, without copying a scheduler or assuming CRI behavior.
- Capture the original child start arguments, actual entry/GP/priority, first blocking API and original parent return. The later retail result establishes completion but not the intervening child lifecycle.
- Define priority dispatch and preserve the active borrowed parent CPU, stack and RPC queue while the child runs. Restore current-thread ownership on yield and failure; distinguish a supported child wait from a failed service.
- Trace the child's bounded initialization to its first wait, including actual imports, callbacks and any SPU2/timer or further thread dependencies. Reject unrelated preemption until evidenced.
- Verify suspension/return ownership with distinct synthetic parent/child continuations and independently captured retail effects. Then complete original 0x40C result transport and follow 0x422, its local buffer ownership and subsequent requests.

This newly reached scheduling boundary requires its own implementation design. General EE/IOP scheduling, audio playback, stream payload loading and presentation remain incomplete.
