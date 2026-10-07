# EE file I/O through IOP FILEIO

## Why this is a new subsystem

Main's frame step (`0x1002D8`) loads `cdrom0:\BIN\DBZP.BIN;1` with the C library's file API: open (`0x333DB0`, mode 1), two seeks (`0x3341B8`: end for the size, then start), one read of the whole file into an EE buffer (`0x3343F8`), FlushCache, close (`0x334040`). The first open initializes stdio: it creates the stdio lock ("SceStdioFioSema", admitted) and binds the IOP FILEIO server (`0x80000001`, client `0x3AC740`). Every later call is a FILEIO RPC that the IOP serves with the boot FILEIO module, IOMAN and CDVDMAN's `cdrom0` device (ISO9660 lookup plus sector reads). Only boot FILEIO's heap handler is translated today; the bind therefore stops as a checked boundary.

## Evidence needed

- Boot FILEIO's server registration for `0x80000001` (descriptor/buffer) and its open/lseek/read/close handlers, from the IOPRP/boot images already used for the heap handler (`tools/iop/generate_boot_heap.py`).
- IOMAN's device dispatch and CDVDMAN's `cdrom0` device operations (the same CDVDMAN image as the disc service).
- An original capture after the file is read (e.g. after `0x10034C`) to compare the EE buffer, the returned size and FILEIO state.

## Bounded design

1. Translate the boot FILEIO RPC server (open/lseek/read/close and its bind) like the heap handler; validate its registration against captured IOP RAM.
2. Serve IOMAN/`cdrom0` device calls from the original modules where available; where the reached path needs CDVDMAN internals already modelled for CDVDFSV (search, sector reads), reuse those owned models with the same evidence rules.
3. EE side: receivers for the bind and each FILEIO call in main's admitted context (live client ID; the C library's call semaphores), including the SIF DMA of read data into the EE buffer.
4. Compare the loaded `DBZP.BIN` bytes with the local ISO and the original capture; then continue into whatever main does with the loaded image.

## Limits and stop conditions

No host file-system shortcut: reads come from the local ISO through the original handlers or owned models of the reached device calls. Stop if a reached path needs IOP device behaviour without evidence.

## Findings (2026-10-07)

- Bind and initialization run: boot FILEIO's file server descriptor `0x47788` (handler `0x46298` = retail base `0x44430` + `0x1E68`), buffer `0x477D0`, validated in captured IOP RAM. The C library's first FILEIO request `0xFF` (8 bytes: EE result buffers `0x3ABCC0`/`0x3AC100`) completes through the original handler, run directly on an owned stack with the request in its native buffer (`NativeIopService::callFileio`).
- File operations are asynchronous. The open request (function 0, 0x418 bytes: flags, mode and `cdrom0:\BIN\DBZP.BIN;1`) makes the handler allocate a 0xC40-byte request record (AllocSysMemory, mode 1) and hand it to a worker; completion reaches the EE through IOP→EE SIF commands into the buffers registered by request `0xFF`, and the C library waits on a per-open semaphore ("SceStdioOpenSema"). The open now stops before the IOP is entered.
- Original evidence (`rpc-dbzp-loaded`, after main's first close): `DBZP.BIN` (LBA `0x64C`, 0x7D48F bytes) is loaded at EE `0x3B3100`, byte-identical to the ISO. FILEIO's 16 KB read buffer was at IOP `0x1E7100` (last chunk residue); the read data also passes through the CDVD ring at `0x32EF0`, so the `cdrom0` device reads through CDVDMAN's streaming path.

## Next steps

1. Trace the worker side: how the request record is queued (message box/thread), the IOMAN calls it makes, and the SIF command it sends back (command ID, payload, EE handler registered by the C library).
2. Model IOP→EE SIF command delivery into the EE command handler at an owned scheduling point, and the request-record placement (capture or derive it).
3. Serve IOMAN `cdrom0:` from the local ISO through the reached CDVDMAN path; compare the loaded bytes with `rpc-dbzp-loaded`.
