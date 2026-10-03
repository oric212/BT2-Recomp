# Progress log

## 2026-10-03 — Native foundation

- Built and ran the C++20 Windows x64 `bt2-port` executable with Visual Studio 2022/MSVC.
- Added goals, build instructions, content/license rules, and an evidence record.
- Foundation commit: `282edf9794100c7637293e558f9f12a75889fd95`.

## Current work — Local executable validation

- Added `IMPLEMENTATION_PLAN.md` and this log.
- Added a read-only Python validator for the local filename, basic ELF header, optional SYSTEM.CNF boot reference, and SHA-256.
- Local preliminary validation passed for `rom/SLUS_214.41`: 2,675,936 bytes; ELF32 little-endian MIPS executable; boot reference `cdrom0:\SLUS_214.41;1`.
- SHA-256: `f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e`.
- Verified the validator using synthetic files: accepted header, matching reference hash, rejected mismatched hash, wrong boot reference, truncated header, wrong byte order, and wrong machine type.
- Consolidated README content under `docs/` and removed the duplicate progress document; this is the sole progress log.
- Retail revision confirmation requires a trusted reference hash. No game functions or runtime behavior have been analyzed.

## 2026-10-03 — ELF metadata inspection and documentation cleanup

- Moved the plan and this log under `docs/`; merged redundant project goals into the plan and consolidated tool instructions in `TOOLS.md`.
- Added an ELF32 metadata inspector and synthetic tests. Raw reports remain under ignored `work/`.
- All seven synthetic tests passed: load/NOBITS metadata, truncated tables, out-of-bounds payload, invalid load sizes, invalid name offset, empty-segment warning, and unsupported extended numbering.
- Observed entry point `0x00100008`, four program headers, and 33 section headers in the local executable with the hash recorded above.
- The declared entry is inside an executable load region. No game code has executed.
- Preserved an empty load record and flagged its generic alignment anomaly; its purpose is unknown.

Next: assess PS2Recomp requirements and licensing against this executable metadata, then establish an integration plan. Trusted retail revision confirmation remains pending.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation.
- [ ] Trusted retail revision confirmation.
- [x] ELF metadata inspection (alignment anomaly recorded).
- [ ] PS2Recomp integration.
- [ ] Game entry point reached.
- [ ] First rendered frame.
- [ ] Title screen.
- [ ] Main menu.
- [ ] Character select.
- [ ] Battle load.
- [ ] Playable versus battle.
- [ ] Audio.
- [ ] Full match.
