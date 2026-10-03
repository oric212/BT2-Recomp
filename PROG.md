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
- Retail revision confirmation requires a trusted reference hash. No game functions, memory regions, or runtime behavior have been analyzed.

Next: inspect ELF metadata after preliminary validation, keeping raw output outside Git.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation.
- [ ] Trusted retail revision confirmation.
- [ ] ELF inspection.
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
