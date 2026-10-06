# CRI first AutoDMA transfer boundary

## Reached evidence

The 0x40C handler now starts its higher-priority child, which parks at the original first WaitSema. LIBSD then reaches its first core-0 transfer setup. Independent isolated captures preserve original StartThread and WaitSema calls, observe StartThread return 0, and record the original CHCR load at retail 0x84E80 (native 0x92250): register 0xBF8010C8, value 0. Transfer arguments are core 0, mode 0xC000, buffer 0xC6EF0, size 0x1000. The relocated native buffer is 0x176FC0; both observed 4096-byte buffers contain zeros, SHA-256 ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7.

A second terminal observer reaches the original requested ADMAS halfword store at retail 0x859E8 / native 0x92DB8: address 0xBF9001B0, value 1. The observer records the operands without issuing the store. Native owned configuration remains ADMAS 0 and rejects the same write. No AutoDMA transaction, completion, IRQ, CRI 0x40C reply or native audio playback is supplied.

## Required transfer design/evidence

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP/audio ownership patterns, pinned PCSX2 SPU2 Dma/ReadInput/RegTable and public PS2SDK LIBSD/DMA contracts. Adapt behavior to the actual BT2 setup; do not copy a renderer/audio engine or infer mode semantics from BT3.
- Capture the original subsequent MADR/BCR/CHCR programming, interrupt enable state, actual transfer API return and first completion/callback. Distinguish initial configuration from active transport and continuous refill.
- Define ownership of the static initial buffer, transfer byte count, halves/loop behavior if confirmed, and SPU2 input destination. Keep IOP RAM, SPU2 storage and host audio output separate. Do not equate a logical transfer or zero-filled initial buffer with audible game content.
- Implement the bounded initial accepted transaction and its interrupt/callback lifecycle. Preserve nested interrupt state, the parked CRI child, pending RPC parent and server queue. Reject unsupported transfer modes, out-of-range buffers and later refill until observed.
- Complete original 0x40C return/EE cleanup only after its real handler returns; then follow captured 0x422 and further startup requests at subsystem scale.

The existing eight-halfword FIFO initialization is separate from this first AutoDMA path. General playback, voice mixing, audio timing, child semaphore wake and cross-CPU scheduling remain incomplete.
