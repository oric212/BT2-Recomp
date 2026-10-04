# Bounded SPU2 initialization

Continue the actual CRI_ADXI -> LIBSD sceSdInit path. Existing library registration does not initialize SPU2 or establish audio playback.

1. Correct the initial diagnostic: PC 0x91614 writes halfword 0x0900 to physical 0x1F9007C6 (SPDIF mode); it is not a read.
2. Consult pinned SDK register definitions and PCSX2 handling, then retain only reached configuration writes with their actual semantics. Unknown widths/addresses/readback remain rejected.
3. Run after each meaningful stage. At status/transfer boundaries, capture the actual reached local hardware value or transition with guarded instrumentation; never return arbitrary ready/zero values.
4. Complete the bounded initialization sequence where evidence permits, retain owned interrupt/DMA/callback state, and compare local startup effects before acknowledging the pending module.
5. Continue later startup blockers in the same scope. Sample processing/playback, unsupported transfers and new subsystems need separate evidence/plans. Test, update progress/history, commit and push at the end.

All local captures, instrumented binaries and generated retail-derived source remain ignored. Adapt public/reference patterns, do not copy implementations.


## Verified boundary

Reset configuration, the two observed status polls, release-command retention, modulation clears, IRQ teardown, descriptor clearing and effects/transfer address setup execute. Pending CRI_ADXI remains nonresident. First data-FIFO write: halfword 0x0700 to 0x1F9001AC at PC 0x940CC.

The next scope needs a sound RAM transaction model: capture the reached FIFO sequence, ATTR transfer-mode transition, status/completion and destination bytes; compare those with PCSX2 handling before accepting the initialization transfer. Later DMA, IRQ delivery and voice execution remain distinct unverified behavior. This document does not establish completion semantics for them.
