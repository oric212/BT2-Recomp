# SIO2 card DMA scope

Status: new transfer subsystem; design/evidence work required before starting DMA.

## Established boundary

The real initial MCSERV command-1 handler reaches MCMAN and SIO2. Bounded higher-priority worker wake and initial send-slice programming complete. Current native stop: dmacman:32 / sceStartDMA, IOP PC 0x10E30. Channel 11 retains MADR 0x32020 / BCR 0x10024 / CHCR 0x201. The command buffer is 144 bytes: 81 11 followed by zero padding. Channel 12 is not configured or completed. No bytes have been transferred, no SIO2 IRQ synthesized and the RPC request remains pending.

Independent interpreter evidence invokes actual sceSetSliceDMA, captures all five arguments, return 1, physical register readbacks and stable source bytes. Relocated MCMAN code validates the source's offset 0x12020. Public pinned PS2SDK dmacman.c/h define slice size/count and START separately. Existing byte-FIFO controller replies cannot stand in for memory-card DMA replies.

## Design questions to resolve

- Exact command-queue lengths versus padded DMA input, receive slice, direction and destination ownership.
- Source consumption and receive completion ordering, channel START/CHCR readback and IRQ 17 delivery through the actual registered worker.
- Device policy for an absent card versus a mounted local card; do not bake the callback capture's -2 result into the runtime or infer card state from unrequested fields.
- Actual MCSERV secondary SIF descriptor and poll after SIO2 completion, followed by real primary reply and guest callback. The 192-byte local stack includes uninitialized fields; copy actual owned native bytes rather than retail snapshots.
- Callback FUN_0031DD60 is an observed indirect entry absent from the current provisional EE map. It needs evidence-backed discovery/generation when execution reaches it, not a handwritten success stub.

Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP/DMA interfaces, PS2SDK dmacman and PCSX2 Sio2/Dmac code before selecting the bounded transfer model. Capture the first send/receive and device-profile effects locally. No full PS2 emulator, arbitrary card filesystem, or permissive DMA success is part of this scope.
