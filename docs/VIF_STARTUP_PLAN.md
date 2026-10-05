# Initial VIF/VU command startup

## Reached boundary

Generated startup completes the embedded `ck01` entry, nonresident cleanup, source heap free and three global DMAC setup writes. It then calls FUN_0030FF60 from EE PC 0x1005B4. The first unsupported operation is the 32-bit store at PC 0x30FF70 to VIF1 FBRST, address 0x10003C10, value 1. No VIF reset or FIFO command succeeds yet.

The reached routine subsequently writes VIF1 ERR, accesses the VU control register through CFC2/CTC2, reads two quadwords from local EE data at 0x3680A0/0x3680B0, writes VIF1 FIFO at 0x10005000 twice and writes GIF CTRL. These are observed generated instructions, not proof of completed hardware effects. Generated ABI storage and checked RAM quadword adapters permit compilation; they do not implement VU execution.

## Bounded next pass

1. Consult REFERENCE_ARCHITECTURE.md, BT3's runtime/VU interfaces and pinned PCSX2 VIF/VU/GIF reset and FIFO handling. Adapt only the reached behavior.
2. Capture the unique caller return at 0x1005BC and, where needed, actual VIF/GIF/VU register reads before/after each reset. Preserve source ELF, disc, emulator and BIOS hashes.
3. Decode the two local FIFO quadwords with bounded tooling. Keep raw packets and proprietary-derived output ignored; do not assume they contain geometry, microcode or drawing commands before inspection.
4. Implement the reset/control state and exact command set required by those packets. Model reset side effects and routing to the existing GIF consumer where evidence supports it. Reject unsupported commands and avoid fabricated completion/readbacks.
5. Generate/link/run and continue through related VIF/GIF startup blockers. If real VU microprogram execution or a genuinely new rendering subsystem is reached, establish its own evidence/plan rather than inventing success.
6. Build/test, compare independent captures, update PROG/history, commit and push.

Full VIF, VU microprogram execution, hardware DMA scheduling and presentation are not supplied by this plan.
