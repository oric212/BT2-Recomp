# Second CRI channel and IOP address-space capacity

## Confirmed frontier

The first channel completes command 0x40C, a fourth heap allocation and sixteen original 0x422/0x400 constructor pairs. Current full native execution stops at **selector 0x77 / PC 0x32F028**, pending FILEIO heap operation 1: RPC ID **90**, client **0x3AC840**, input **0x3AC8C0**, requested size **0x8D0**, output **0x3AC880**. Four allocations are complete; this fifth request is not accepted.

An independent original call-36 capture already reaches the subsequent CRI command 2 with input `[1, 0x35FD00, 0x191740, 0x880]`. The retained heap input/result is size 0x8D0 / pointer **0x191700**. With the verified 256-byte rounding, that next retail interval is **[0x191700, 0x192000)**. Our translated FILEIO image currently owns **[0x191700, 0x195980)**. Accepting the allocation at this layout would violate ownership. Do not overwrite FILEIO or change the EE-visible heap result.

The call-36 snapshot and prior completed call-35 result establish a second channel construction handoff. They do not establish its later commands, further allocation sizes, a second AutoDMA transfer, audio consumption or asset reads.

## Required broader layout work

- Follow REFERENCE_ARCHITECTURE.md and IOP_HEAP_CAPACITY_PLAN.md. Inventory every translated image, export/import/data region, captured heap, disc workspace and thread/call stack together. The current fourth-heap layout is verified but leaves no room at the next captured pointer.
- Gather subsequent second-channel allocation evidence before selecting placements, to avoid moving one image for each new request. Assess compact placements in the existing 2 MiB address space and any low-memory ABI assumptions; do not infer free ranges from a retail capture alone or introduce a larger emulated RAM default.
- Choose and document a consistent ownership layout. Regenerate affected private images, update all owned imports/GP/context/address gates and rerun independent normalized module comparisons. Preserve original captured heap pointers and all first-channel behavior.
- Admit the fifth operation only after its complete rounded interval is disjoint. Keep epoch, phase, overlap and no-mutation rejection tests. Then execute the original second-channel handler and follow its bounded request group through generated cleanup.
- Keep the first AutoDMA transaction pending until independently established consumption/completion behavior is implemented. Never replace an original callback or second-channel result with captured success bytes.

This is the next address-space/design scope. General IOP allocation, cross-CPU scheduling, audio output, archive payload loading and presentation remain unfinished.
