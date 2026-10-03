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

## 2026-10-03 — PS2Recomp tooling

- Pinned PS2Recomp at `c5a9d02573410a2085a4b4b831b0b68ba3515440` and six dependencies in `config/ps2recomp.lock.json`; recorded inspected license files in `LEGAL.md`.
- Built the recompiler and analyzer with Visual Studio 2022/MSVC x64, with upstream runtime, studio, and tests disabled.
- Added reproducible build, synthetic translation, and guarded BT2 generation scripts. Source checkouts, build outputs, and generated output stay ignored.
- Synthetic translation recompiled one self-authored function with zero decode failures, unhandled instructions, warnings, or errors. Generated C++ has not been compiled or executed.
- Function-map tests passed, including the actual exporter header, neutral naming, range validation, empty maps, and duplicates. Missing-map generation stops before producing BT2 output.
- Optional zlib/zstd dependencies were not found, so compressed DWARF support was not established. fmt emitted a C4834 warning on the initial build.
- `README.md` stays at the repository root; the remaining documentation stays under `docs/`.

Next: establish reviewed Ghidra function boundaries for the local executable, then run local generation. No BT2-derived C++ has been generated and no upstream runtime has been linked. Trusted retail revision confirmation remains pending.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation.
- [ ] Trusted retail revision confirmation.
- [x] ELF metadata inspection (alignment anomaly recorded).
- [x] Pinned PS2Recomp tools build and synthetic translation.
- [ ] BT2 generation and custom runtime integration.
- [ ] Game entry point reached.
- [ ] First rendered frame.
- [ ] Title screen.
- [ ] Main menu.
- [ ] Character select.
- [ ] Battle load.
- [ ] Playable versus battle.
- [ ] Audio.
- [ ] Full match.

## 2026-10-03 — Ghidra setup and project path rename

- Renamed the parent directory from `D:\Cs\C++` to `D:\Cs\Cpp` at the user's request; project root is now `D:\Cs\Cpp\BT2-Recomp`. README remains at the root.
- Installed Ghidra 12.1.4 under `D:\Cs\Cpp\tools`, verified the official archive hash, and built the pinned R5900 extension v2.1.37 against it. Added tool locks and an authored headless analysis/export workflow.
- Imported the unchanged local executable with `r5900:LE:32:default`. Exported 9,541 unreviewed function candidates, including 7,241 contiguous aligned CSV candidates. All proprietary-derived reports and projects remain ignored.
- Ghidra reported a conflicting `rom0` block, DVP overlay byte-mapping limitations, and decompiler flow errors. No function boundary or purpose is certified by this pass.
- Rebuilt the native host and pinned PS2Recomp tools at the new path. The host ran with the expected startup output; synthetic translation passed with zero warnings, errors, decode failures, or unhandled instructions.
- After each completed pass, review, commit, and push as requested by the user.

Next: review the entry candidate boundary and control flow before producing a reviewed map. No BT2-derived C++ has been generated or executed.

## 2026-10-03 — Entry control and boundary review

- Added an authored static audit that reads instruction words through the ELF executable section and compares control targets against the hash-matched R5900 export. Reports remain ignored.
- Three synthetic tests passed, covering signed branch offsets, jump-region arithmetic, indirect/syscall encodings, unsupported branches, target mismatches, and missing delay-slot coverage.
- The local entry audit passed: 144 instruction addresses, 16 transfers, all direct targets matching, conditional branches internal, and delay slots present.
- Reviewed the entry's static body extent `[0x00100008, 0x00100248)`: final external jump at `0x00100240`, followed by its included delay slot. Recorded neutral direct targets and two syscall sites without inventing purposes or ABI behavior.
- Full opcode semantics, callee boundaries, syscall meanings, runtime behavior, and a complete generation map remain unverified. No game code generated or executed.

Next: export and review the entry's direct callees and final jump target, then establish the observed syscall ABI from primary evidence.

## 2026-10-03 — Direct entry target review

- Extended the authored Ghidra exporter to include complete instruction listings for the five direct entry targets. Raw listings remain ignored.
- Extended control decoding to integer branch and branch-likely families, with explicit likely metadata; no delay-slot execution or annulment is simulated.
- All five target bodies passed limited static audits: 320 instruction addresses and 96 transfers total. Recorded supported body extents, return/jump endings, and unresolved indirect calls in `REVERSE_ENGINEERING.md`.
- Found a direct call at `0x00100688` to `0x003B7FA8`, outside file-backed executable sections. No contents or purpose inferred and no stub added.
- Recorded a third syscall site, with preceding `v1 = 0x64`; semantic names and ABI remain unverified. Preserved neutral names despite a Ghidra semantic label.
- Synthetic checks and the entry audit passed after extending the decoder. No BT2 C++ generated or executed.

Next: confirm syscall ABI from primary evidence and investigate the unmapped direct target before extending local generation.

## 2026-10-03 — Startup ABI and unmapped target evidence

- Inspected pinned PS2SDK primary sources; recorded revision and file hashes in `config/startup_abi.sources.json`. Public selector/wrapper conventions match SetupThread (`0x3C`), SetupHeap (`0x3D`), and FlushCache (`0x64`) at the observed local sites. Runtime behavior and sentinel handling remain untested.
- Added an authored, read-only startup audit. Raw instruction checks passed for all three selector/syscall pairs and zero-loop constants.
- Confirmed `0x003B7FA8` lies outside every declared load memory extent but inside the apparent entry zero-store interval `[0x00387E80, 0x0043058F)`. Found one encoded JAL and no aligned exact target literal in allocated file-backed sections.
- Recorded the limits of those scans: split constants, calculated addresses, subsequent writes, and actual target contents remain unresolved. No function body or stub was invented.
- Reports and downloaded reference sources remain ignored. No guest code generated or executed and no runtime syscall implementation added.

Next: trace the writer or copy operation that could populate the target before its call, using local PCSX2 memory observation if needed.
