# Memory cards and saves

## Current model: absent cards

Memory card requests run through the original IOP modules (MCSERV, MCMAN, SIO2MAN). On the SIO2 bus, the card ports answer as an empty slot. This is the device policy `sio2_card_absent` in `runtime/iop/core/native_runtime.h`.

**Startup query.** MCSERV's GetInfo for port 0 / slot 0 calls MCMAN's card detection. Because the card type stays "no card", every detection runs the PS2 probe and then the PS1 probe, matching the public `mcman_detectcard`:
- five `0x11` DMA probe packets and one `0xF3` authentication packet, each 144 bytes;
- a 300 µs delay;
- five `81 52 00` FIFO probe packets, each guarded by a 100 µs alarm.

**Frame-phase queries (2026-10-08).** After the controller check, the game polls the card every frame with a no-wait GetInfo that requests the type, free space and format (end callback `0x31DD60`, end parameter `0x3A7480`). Each query repeats the probe sequence above, so the card path checks its counters relative to the start of each query.

Two pieces of state carry over between queries:
- **Pending alarm.** The last timeout alarm of a query is still pending when MCSERV returns. On hardware it fires 100 µs later and signals the card semaphore. Logical IOP time therefore advances by one field at each VBlank, and a pending card alarm fires then, in interrupt context.
- **Restart write.** SIO2MAN's next transfer writes `ctrl|0xC`. The control register still reads back `0x3BD`, so that write is a restart over the consumed reply of the previous query's last packet.

**Original comparison.** The capture `rpc-mc-frame-callback`, taken at the first frame-phase end callback, shows:
- reply −11;
- type 0, free 0;
- end-parameter argument `0x3A7480`.

`verify_mc_frame_query.py` compares it with the native report.

## Later: real cards and save import (requested)

Players must be able to bring saves from anywhere:
- **Emulator card images:** PCSX2 `.ps2` 8 MB images and other emulators' raw card dumps.
- **Saves exported from an original console:** the common exchange formats `.psu`, `.max` (Action Replay Max), `.cbs` (CodeBreaker), `.xps` (X-Port) and `.sps` (SharkPort).
- **Saves downloaded from the internet:** in the same formats.

The port does not need to work the way PCSX2 does. The design:

1. **Card backend.** A mounted-card device answering the SIO2 card protocol (PS2 card commands, page reads/writes, erase, ECC), backed by an 8 MB card image on the host. It replaces the absent-card policy per slot.
2. **Import/export layer.** Converts single-save exchange formats into directory entries on the virtual card, and exports them back out. It validates headers and checksums, and refuses unknown formats instead of guessing.
3. **Persistence.** Writes the host card file atomically after each guest write sequence, so that a crash can't corrupt a save.
4. **Verification.**
   - Compare format, directory and file results from GetInfo and GetDir with PCSX2 captures that use a formatted card and a card holding a BT2 save.
   - Round-trip each exchange format through import and export.

This begins once gameplay reaches the save menus. Until then, frames keep using absent cards for repeatability.
