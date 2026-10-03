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
- [x] Local provisional BT2 EE map and C++ generation.
- [x] Isolated generated BT2 routine compiled, linked and executed.
- [ ] Full generated-code/runtime integration.
- [x] Generated game entry reached (stops at SetupThread; no complete boot).
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

## 2026-10-03 — Reference-guided recompilation pipeline and real-code probe

- Inspected BT3-Recomp at `e22a34beb564e2d0f32bd60b02f0a95d9645ddcc` as primary reference and Chronicle at `05fd37be122cd5833cfeeceb83475933959d938f` for validation/symbol/PCSX2 practices. Recorded architectural decisions and source links in `REFERENCE_ARCHITECTURE.md`. No foreign game addresses, assets, or patches were adopted.
- Changed strategy to automated provisional mapping -> generation -> compile/link -> execution -> observed blocker -> narrow runtime/patch work. Existing manual entry audits remain tooling validation; exhaustive manual boundary review is no longer a generation gate.
- Prepared 7,241 neutral EE `.text` rows automatically; quarantined 2,300 noncontiguous/invalid bodies. Selected coverage is 1,050,284 bytes (about 45% of declared `.text`), not a complete code map. Exclusions, maps and manifests remain ignored.
- PS2Recomp generated 7,241 functions: zero decode failures, zero unhandled instructions, zero errors, 432 indirect-flow warnings and 17,654 promoted fallback entries. Warnings remain unresolved diagnostics, not correctness certification.
- Added one-command orchestration, executable/map/tool/output provenance, stale-output archiving, an optional MSVC/C++20 generated probe, and separate entry compile diagnostics.
- The real generated `FUN_0032EEE0` compiled, linked and executed through an authored minimal adapter. Verified syscall selector `0x64`, operation zero, resume PC, return sentinel and register preservation. Unsupported syscall/cache/target/return cases were rejected. Cache requests are recorded only; no PS2 cache model or boot claim.
- CTest passed the generated probe; all 11 synthetic map/provenance tests passed, including modified output, stale source and changed-lock rejection. The normal host rebuilt and ran. MSVC emitted C4702 for an unreachable generated epilogue; no generated-source patch applied.
- Entry compilation fails on missing `GPR_VEC`, `SET_GPR_VEC`, `ps2_paddu32` and broader context/runtime APIs. This is the actual next integration blocker. The game entry has not executed; the entire generated set has not compiled/linked.

Next: extend the compatibility API enough to compile/link and attempt entry execution, then investigate only the paths that block it. Keep generated game output ignored and repeat the pipeline after each change.

## 2026-10-03 — Generated entry execution to first syscall

- Extended the authored bootstrap context and helpers required by emitted entry code: 128-bit GPR access/unsigned saturated lane addition, scalar writes, HI/LO/SA/FPR state, zero-operand FPU addition, and checked byte/quadword stores. Broader FPU, MMIO, TLB and scheduling behavior remains unsupported.
- Prepared a bounded 32 MiB RAM image from validated PT_LOAD metadata, copying file bytes and zero-filling declared memory. Added bounds/overlap checks and ignored image provenance.
- Added a deterministic compiler-facing entry copy that routes its one emitted direct tail call through a checked runtime hook. Original recompiler output remains unchanged; prepared retail-derived code stays ignored. No missing game function body was invented.
- Entry compiled and linked with MSVC/C++20. Executed actual generated startup instructions, verified CPU state, and observed 43,120 quadword stores plus 15 byte stores clearing `[0x00387E80, 0x0043058F)`. The probe deliberately poisoned that uninitialized region to verify stores and checked its boundary bytes.
- Stopped at unsupported SetupThread selector `0x3C`, resume PC `0x001001CC`, with arguments matching the earlier static review. Captured ignored `entry-stop.json`. No syscall return or guest stack was fabricated.
- All 14 Python tooling tests and three CTests passed: original generated wrapper, synthetic bootstrap helpers, and entry-to-syscall execution. Generated entry retains C4702 unreachable-code warnings; generated sources were not manually edited.
- The normal host remains independent. Game boot, SetupThread/SetupHeap implementation, deeper callees and full generated-set integration remain incomplete.

Next: establish and implement SetupThread's observed sentinel/stack behavior, then repeat execution to discover the next blocker.

## 2026-10-03 - SetupThread bootstrap model and next syscall

- Added a bounded, single-initialization SetupThread adapter for the observed default-stack sentinel. Explicit stacks, invalid sizes/alignment, repeated setup and invalid argument memory remain unsupported.
- Used Play! revision `83700b2c31e593bc94e845b4b31b797be84dda59`, `Source/ee/PS2OS.cpp` (`sc_SetupThread`) and `PS2OS.h` (`STACKRES`) as the behavioral model: RAM minus 4 KiB is stack top; reserve `0x2A0` for context. Source hashes are recorded in `config/startup_abi.sources.json`; reference downloads stay ignored. No emulator implementation was copied or linked.
- For BT2's 1 MiB request, stack base is `0x01EFF000` and initial SP is `0x01FFED60`. Empty launch policy writes argc=0 and argv[0]=null to checked guest memory. This is a compatibility model, not confirmed retail BIOS behavior; scheduler/root execution remains unsupported.
- Re-executed real generated entry. CPU clearing and 43,120 quadword/15 byte stores still verified. Entry consumed the returned SP and reached unsupported SetupHeap selector `0x3D`, resume PC `0x001001E8`, arguments `0x0043058F`, `0xFFFFFFFF`.
- MSVC build and all three CTests passed. Synthetic checks cover invalid sizes, repeated initialization, empty argv bounds and preserved upper return-register bits. Original generated source remains unchanged and ignored; game boot remains blocked.

Next: implement the reached SetupHeap contract, then repeat generation/compile/execution diagnostics.

## 2026-10-03 - SetupHeap and first generated guest call

- Implemented the observed SetupHeap default-size sentinel using the already pinned Play! `sc_SetupHeap` model: return the initialized thread's stack base as heap end. Recorded the requested start `0x0043058F` and returned end `0x01EFF000` without silently aligning the caller's value. No allocator or heap memory initialization is claimed.
- Reject setup before thread initialization, repeated initialization, explicit heap sizes and starts at/beyond stack base. Preserve upper 64 bits of the return GPR; bounds checks remain active.
- Actual generated entry consumed both startup models and reached a direct call to neutral `FUN_00338520`, source PC `0x001001E8`, resume PC `0x001001F0`. Guest call dispatch remains unsupported; no body was skipped or stubbed.
- Added structured guest-branch diagnostics and updated the pipeline to report either syscall or dispatch stops. The ignored entry report records target/source/resume PCs and game_booted=false.
- MSVC builds and three CTests passed; all 14 Python tooling tests passed. CPU initialization and 43,120 quadword plus 15 byte zero stores still verified. The independent host runs; generated code, local executable and reports remain ignored.

Next: compile/link the already generated reached callee and implement checked call/return dispatch, then execute to the next actual blocker. Retail BIOS equivalence remains unverified.

## 2026-10-03 - First generated callee and checked guest dispatch

- Compiled/linked the existing neutral generated `FUN_00338520` and registered it alongside entry. Runtime preparation deterministically routes its emitted direct tail call through the dispatch hook, just as for entry; original generated files remain unchanged and ignored. Preparation records hashes for both compiler-facing copies.
- Added bounded call/return dispatch for registered direct/indirect calls and tail targets, with nested return frames, a 64-level depth limit, return-PC validation, and exception cleanup. Unregistered targets still stop with structured source/target/resume diagnostics.
- Added checked aligned 64-bit guest reads/writes required by the callee's stack operations. No general MMIO/TLB behavior added.
- Executed real entry into `FUN_00338520`: SP changed from `0x01FFED60` to `0x01FFED50`; one 64-bit stack store saved the caller return address `0x001001F0`. Then stopped at nested JAL source `0x00338528`, target `FUN_003382D0`, return PC `0x00338530`. The nested target has not executed; no function was skipped or stubbed.
- MSVC builds, all three CTests and all 14 Python tests passed. Synthetic checks exercise nested indirect calls, tail calls, invalid returns, cleanup after a failed callee, and doubleword memory bounds/alignment. The generated wrapper and independent host still run; CPU startup and zero-store checks remain intact.

Next: compile/link/register the reached nested target and repeat the execution-led loop. Full generated-set integration and game boot remain incomplete.

## 2026-10-03 - Nested generated startup reaches CreateSema

- Linked/registered neutral `FUN_003382D0` and its reached generated syscall wrapper `FUN_0032EC60`. Both compile directly from original ignored output; neither needed a tail-call transformation or manual source changes.
- Added aligned, bounded 32-bit guest stores. Generated FAST_WRITE32 uses the same checked store API; optional guest-write diagnostic hook has no emulation effect. Tests verify value, adjacent byte preservation, bounds and alignment rejection.
- Executed the nested prologue and six word stores, including the call delay-slot store. Verified SP=`0x01FFED00`, outer saved RA=`0x001001F0`, nested saved RA=`0x00338530`, and the observed parameter words/pointers without assigning unsupported structure meanings.
- Three guest calls now execute: entry to `FUN_00338520`, then `FUN_003382D0`, then `FUN_0032EC60`. The wrapper sets selector `0x40`; pinned PS2SDK `__NR_CreateSema` identifies this boundary. Stopped at resume PC `0x0032EC68`, a0=`0x01FFED00`. No semaphore return/state was fabricated, and the enclosing functions retain neutral names.
- MSVC pipeline/build, three CTests and all 14 Python tooling tests passed. CPU clearing/43,120 quadword plus 15 byte zero stores still verified. Host output remains unchanged. Game data, generated sources and execution reports remain ignored.

Next: implement the observed CreateSema syscall with explicit validated state, then repeat execution to the next blocker. Game boot remains incomplete.

## 2026-10-03 - CreateSema state and real nested returns

- Implemented bounded CreateSema for zero attributes using pinned PS2SDK parameter offsets and Play! creation behavior. Reads max_count/init_count/option, ignores output-only count/wait_threads, and stores explicit count/maximum/option state. IDs begin at zero; capacity is 256 as in the pinned Play! model. No wait queues, wakeups, deletion, signal or scheduler behavior is claimed.
- Reject unsupported attributes and invalid memory/alignment explicitly. Invalid count ranges and exhausted capacity return signed -1 without allocating. Return writes preserve the upper 64 bits of the GPR. These error restrictions are bootstrap policy, not confirmed retail error codes.
- Actual generated startup creates two count=1/max=1 objects, options `0x00383BF0` and `0x00383C00`. It stores IDs 0/1 at `0x0036AB58`/`0x0036AB5C`; generated FAST_WRITE32 uses the bounded store adapter.
- Verified three real generated return paths: both syscall wrapper calls return, then `FUN_003382D0` restores RA and SP and returns to `FUN_00338520`. Execution stops at its next JAL: target `FUN_00338410`, source `0x00338530`, resume `0x00338538`; SP restored to `0x01FFED50`.
- MSVC build/pipeline, three CTests and all 14 Python tooling tests passed. Synthetic semaphore tests cover state/IDs, ignored output fields, preserved upper bits, invalid count, unsupported attributes, parameter memory bounds/alignment and capacity exhaustion. Independent host runs; proprietary-derived files remain ignored.

Next: integrate the reached `FUN_00338410` and repeat the execution-led loop. Game boot and retail BIOS equivalence remain incomplete.

## 2026-10-03 - Generated registration path reaches FUN_003383C8

- Linked/registered original ignored `FUN_00338410` and its reached wrapper `FUN_00338510`. Added FAST_READ32 as an alias to bounded aligned reads. No generated function bodies were changed or skipped.
- First executed to SetSyscall selector `0x74`, resume `0x00338518`. Then implemented a bounded 256-entry registration table using pinned Play! `sc_SetSyscall`; PS2SDK declares SetSyscall void, so v0 is preserved. Reject out-of-range selectors, misaligned/out-of-RAM handlers. Custom handler execution remains explicitly unsupported, including overrides of built-in selectors; no fake invocation result.
- Real generated code registered selector `0x83` -> `0x00338388` and `0x5A` -> `0x00338350`, values read from ELF-loaded words at `0x0036AB48..0x0036AB57`. Both generated wrappers returned.
- Executed `FUN_00338410` prologue, eight additional doubleword saves, and call delay slots. SP=`0x01FFECD0`; saved RA=`0x00338538`. Total seven guest calls/five returns, two semaphore objects, ten doubleword and eight word stores.
- Next observed unregistered target is neutral `FUN_003383C8`, source `0x00338464`, return PC `0x0033846C`, arguments `0x80000000`, `0x80080000`, `0x00338388`. No memory alias, mapping or handler purpose is inferred from those arguments alone.
- MSVC pipeline/build, three CTests and all 14 Python tooling tests passed. Registration tests check state, preserved return register, invalid selector/address/alignment and unsupported invocation. CPU/zeroing/semaphore checks remain active. Independent host runs; proprietary output remains ignored.

Next: compile/link/register the reached target and resolve only the compatibility behavior actually needed. Game boot remains incomplete.

## 2026-10-03 - Registered syscall invocation exposes a map gap

- Linked/registered original generated `FUN_003383C8`. It sets v1=`0x83` and invokes syscall at `0x003383CC`, resume `0x003383D0`. The registered handler is `0x00338388`; invocation remains unsupported, with no handler body skipped or result fabricated.
- Extended syscall-stop diagnostics to include the registered handler address. Verified eight guest calls, five returns and prior CPU/zeroing/semaphore/registration checks. The new wrapper has no extra memory stores.
- Confirmed no `0x00338388` entry in selected map or Ghidra export and no generated function for that address. This is a discovery gap, rather than an excluded candidate. Neighboring functions alone do not establish its boundary.
- Added reusable `inspect_blocker.py`, automatically invoked after successful pipeline execution, to classify registered handlers/direct targets as mapped entries, function interiors or map gaps and retain candidate/quarantine evidence. Reports stay ignored. The tool does not infer boundaries or alter maps.
- MSVC pipeline/build and three CTests passed; all 16 Python tests passed, including classifier exclusive-end and quarantine cases. Runtime test verifies selector/handler/resume diagnostics. Independent host runs; game files and generated output remain ignored.

Next: focused automated Ghidra discovery at the actually reached missing handler, then provisional map regeneration and compile/execute. Custom syscall execution contract and game boot remain incomplete.

### Function count snapshot (before handler discovery)

7,241 functions generated; 8 distinct functions compiled/executed. Four completed and returned (`FUN_0032EEE0`, `FUN_0032EC60`, `FUN_003382D0`, `FUN_00338510`); four executed partially. These are startup/helper paths; gameplay and game boot do not work yet.

## 2026-10-03 — Observed handler discovered, regenerated and entered

- Added `--discover-blocker` to the Ghidra driver and a neutral-name discovery script for one latest execution-observed gap. Guarded aligned initialized executable .text targets, existing-function overlap, matching stop/diagnostic address and fresh export containing the requested entry. No guessed manual map row or semantic name added.
- Ghidra discovered provisional contiguous handler `[0x00338388, 0x003383C8)`, 64 bytes. Regenerated 7,242 functions (7,245 C++/header files) from the updated automated map; proprietary output and previous snapshots remain ignored.
- Linked/registered the discovered handler. Added a narrow native callback contract preserving wrapper RA and checking a sentinel return; this does not model BIOS exception frames, privilege transitions or ERET. Synthetic successful callback verifies RA/PC restoration; unregistered handlers still stop explicitly.
- Generated handler's first instruction attempts a word read at `0x80000000`, PC `0x00338388`. Added structured memory diagnostics and verified this exact unsupported boundary. No alias mapping or BIOS memory contents were fabricated. Nine guest calls and one custom callback are attempted; previous startup checks remain active.
- MSVC pipeline/build and three CTests passed; all 16 Python tooling tests passed. Independent host runs; game boot remains incomplete.

**Current counts:** 7,242 generated; 9 distinct compiled/executed (including the isolated wrapper). Four completed/returned; five entered partially. These remain startup/helper paths, not working gameplay.

Next: establish the required kernel-address access and local memory provenance before continuing the handler's read/scan.

## 2026-10-03 — Read-only kernel capture adapter and generated scan tests

- Checked PCSX2 `memMapKernelMem`: KSEG0 at `0x80000000` maps physical address zero. Pinned revision/file hash in `config/startup_abi.sources.json`. PS2Recomp's FindAddress implementation was consulted as a reference, but not adopted as a substitute for BT2's generated handler.
- Added a bounded read-only word view backed only by explicitly supplied snapshot bytes for `[0x80000000, 0x80080004)`. Other aliases, kernel writes, BIOS exception state and MMIO remain unsupported. Without a snapshot, real startup still stops at its first kernel read.
- Generated `FUN_00338388` now completes and returns in synthetic match/no-match tests; verified a match at `0x80000008`, no-match zero, upper-bound read, rejected writes and invalid snapshot sizes. Synthetic words are authored fixtures, not a retail kernel or proof of retail behavior.
- Added `probe_kernel.py` to accept user-captured raw 32 MiB EE RAM, extract only the low `0x80004` bytes, record capture note/source/snapshot hashes in ignored output, and run the compiled handler diagnostic against the real startup arguments. BIOS ROMs/savestates/wrong-size input are rejected. The extra word is needed because emitted code reads at the upper limit before evaluating the loop bound.
- MSVC pipeline/build and four CTests passed; all 18 Python tests passed. No real EE capture was available, so snapshot-driven diagnostic remains unverified on retail memory; game boot has not advanced beyond the original read. Proprietary captures and reports remain ignored.

**Counts:** 7,242 generated; 9 distinct functions entered. Four complete in real startup/isolated-wrapper probes; the discovered handler additionally completes on synthetic data. No gameplay works yet.

Next: supply/capture local EE RAM at the reached handler and validate the scan result before proceeding with kernel patch dependencies.

## 2026-10-03 - Documentation encoding correction

- Repaired two invalid Windows-encoded dash bytes in this document. All tracked text now passes strict UTF-8 decoding and contains no replacement characters. Added a UTF-8 editor rule and explicit UTF-8 reads/writes in the recent tooling.
- Execution progress is unchanged; the next step remains a local PCSX2 EE RAM capture at the reached kernel handler.
