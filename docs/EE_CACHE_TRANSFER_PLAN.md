# Reached EE cache and CRI transfer boundary

## Evidence

After CRI bind and original command-2 allocation, generated FUN_002FCC58 calls FUN_0032F558 at 0x2FCD84 (resume 0x2FCD8C). Arguments are 0x365BC0 and 0x36643F. The candidate calls FUN_0032F4B0, whose reached body contains CACHE 0x10/0x14 on both indexed ways and MFC0 TagLo. The probe has no TagLo field or validated cache-tag contract; experimental linkage therefore fails compilation. These two routines were removed from the private selected bindings to preserve the working diagnostic executable. Neutral map boundaries remain provisional.

This is a new R5900 cache/transfer subsystem. The previous coherent owned-RAM copies do not establish indexed tag reads, valid/dirty bits, operation ordering or the helper's conditional behavior. A missing field initialized to zero would silently choose branches without evidence.

## Scope to establish before implementation

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 cache/transfer patterns and authoritative R5900/PCSX2 CACHE and TagLo semantics. Adapt the ownership contract; do not copy implementations.
- Capture the original unique caller inputs/return, status handoff and actual first CACHE/TagLo observations. Determine whether the bounded native model can use coherent owned RAM and explicit logical tags, or needs dirty-line/alias state. Do not equate physical cache timing.
- Define TagLo ownership and CACHE effects transactionally, including both indexed ways and addresses. Extend the authored ABI/preparation tooling, never the proprietary-derived source by hand.
- Link only the reached routines, run the original helper and compare its memory/interrupt effects. Continue through CRI 0x403 and the captured 0x40C request. Streaming, SPU2/audio effects and archive payload requests need their own observed contracts.

The protocol/hardware design is not yet validated. No native audio or presented game frame follows from current CRI residency/allocation.
