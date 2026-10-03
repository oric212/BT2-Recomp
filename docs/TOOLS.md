# Local executable tools

## Preliminary validation

Requires Python 3.10 or newer; uses only the standard library. From the repository root:

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41
```

The tool reads the local executable without changing it and prints JSON containing size, SHA-256, and preliminary validation results. It checks the filename and basic ELF32 little-endian MIPS executable header. A sibling `SYSTEM.CNF`, when present, must name `SLUS_214.41` in its boot reference. Use `--system-cnf PATH` to supply it explicitly.

Use `--expected-sha256 HASH` to compare against an independently trusted reference. Without that reference, successful preliminary checks do not authenticate the retail revision. This tool does not parse ELF tables, inspect functions, or load the game. Failures return a nonzero exit code.

Keep game inputs under ignored `rom/` or `game_data/` and temporary reports under ignored `work/`.

## ELF metadata inspection

```powershell
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41
```

The inspector reports entry point, program headers, declared load regions, and section headers/names as JSON. Addresses in JSON are decimal integers. It checks table and payload bounds, load sizes, allocated address overflow, and section-name references. SHT_NOBITS sections need no file payload. Generic load alignment anomalies are warnings and are retained in the report. Extended numbering is explicitly unsupported. This is a metadata inspector, not a loader, disassembler, or full ABI conformance checker.

To save a local report:

```powershell
New-Item -ItemType Directory -Path work -Force | Out-Null
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41 | Set-Content work/elf_report.json -Encoding utf8
```

Run synthetic tests without game data:

```powershell
python -m unittest discover -s tools/inspect_elf -p "test_*.py"
```

Field layouts and generic semantics follow the ELF specification: [header](https://gabi.xinuos.com/elf/02-eheader.html), [program headers](https://gabi.xinuos.com/elf/07-pheader.html), and [section headers](https://gabi.xinuos.com/elf/03-sheader.html). Processor-specific values are recorded without guessing their meaning.

## Pinned PS2Recomp tools

Requires Python 3.10+, Git, CMake 3.21+, and Visual Studio 2022/MSVC x64. Internet access is needed to fetch the open-source sources on the first build. The host's normal CMake build stays independent.

```powershell
python tools/ps2recomp/build_tools.py
python tools/ps2recomp/smoke_test.py
python -m unittest discover -s tools/ps2recomp -p "test_*.py"
```

The build checks immutable revisions from `config/ps2recomp.lock.json`, stores clean checkouts in ignored `work/dependencies/`, and builds tools in ignored `build/ps2recomp-tools/`. Upstream runtime, studio, and test targets are disabled. Supplying dependency paths directly to CMake avoids Git submodule shell helpers in the Windows sandbox. Builds run serially with MSBuild node reuse disabled. Local provenance records the lock-file and recompiler hashes.

The smoke test creates our own ELF containing `addiu v0, zero, 42; jr ra; nop` and our own function map, then checks the emitted C++. It does not compile or execute that output and establishes no BT2 function boundaries.

## Local BT2 generation

Automatically prepare a provisional map from our local Ghidra export with `tools/ps2recomp/prepare_map.py`. It retains contiguous aligned file-backed `.text` candidates, neutralizes names, and quarantines overlapping, oversized, noncontiguous or out-of-section candidates. It does not treat VU code as EE code or certify boundaries. Keep maps and exclusion/provenance manifests under ignored `work/`. Manual investigation follows actual build/execution blockers.

```powershell
python tools/ps2recomp/prepare_map.py
python tools/ps2recomp/generate_game.py --candidate-map work/bt2-generation/candidates.csv
```

CSV columns are `Name,Start,End,Size` (case insensitive), with exclusive ends. The driver checks executable identity, ranges/overlaps, candidate manifest, and pinned tool provenance. `--function-map PATH` remains available for explicitly reviewed maps. Matching the hash establishes consistency with our inspected file, not authenticity against a retail reference. Configuration/logs/manifests stay under `work/bt2-generation/`; C++ stays under ignored `game/generated/bt2/`, with automatic instruction patches disabled and no manual stubs or skips. Prior generated output is archived locally before regenerating, preventing stale source files from joining the new set.

These checks cannot certify map correctness. Generated output can include upstream heuristic discoveries and fallback entrypoints; retain warnings as diagnostics. The emitted code uses PS2Recomp runtime APIs. The probe adapter supports one routine; broader integration remains unfinished.

## Generate, build and execute

```powershell
python tools/ps2recomp/run_pipeline.py
python tools/ps2recomp/run_pipeline.py --reuse-generated
```

The first command prepares the map, generates code, hashes all source/header output, prepares runtime input, builds the host and `bt2-generated-probe` with MSVC/C++20, runs tests, then builds and executes `bt2-entry-probe`. Reuse verifies executable/map/tool-lock/recompiler/output hashes and file-set identity before rebuilding. Stages record logs under ignored `work/bt2-generation/`.

The probe registers and executes the actual generated `FUN_0032EEE0` from the broad generation set. It verifies selector/operation, syscall resume PC, return target and preserved register bits. Unsupported syscalls, cache modes, guest targets and returns throw errors. It records the cache request; it does not model caches, DMA, MMIO, graphics, or boot the game. No upstream runtime is linked.

Entry preparation copies ELF loads into a bounded main-RAM image and creates an ignored compiler-facing entry source under `game/generated/bt2-runtime/`, routing emitted direct tail calls through runtime dispatch. The original generated set is unchanged. `runtime-preparation.json` records source/image hashes and hook count. Out-of-range guest memory, unaligned word/quadword accesses, unsupported dispatch and unsupported syscalls throw errors; backedges have an execution budget.

The entry probe poisons its observed uninitialized zero-store interval, executes generated CPU initialization/zeroing, and verifies memory coverage and boundary bytes. It currently expects to stop at SetupThread (`0x3C`) and writes `entry-stop.json` with selector, PC, arguments and store counts. This verifies actual entry execution, not successful boot or a syscall implementation. FPU support is deliberately restricted to entry's zero operands.

`bt2-entry-probe` is excluded from the default build and built explicitly by the pipeline. Its build result and execution stop are reported separately in `pipeline-result.json`, `integration-build.log` and `entry-test.log`; a successful pipeline result means the checked probes passed. The normal host build remains independent.

## Installed Ghidra analysis

Project root: `D:\Cs\Cpp\BT2-Recomp`. Ghidra 12.1.4 is installed at `D:\Cs\Cpp\tools\ghidra_12.1.4_PUBLIC`; launch the GUI with `D:\Cs\Cpp\tools\run-ghidra.bat`. It uses the existing Adoptium JDK 25. Tool versions and archive hashes are recorded in `config/ghidra.lock.json`.

The R5900 extension v2.1.37 was built from pinned source against this Ghidra version. Gradle 9.8.0 is installed beside Ghidra. These tools remain outside the repository.

```powershell
python tools/ghidra/analyze_game.py
```

The driver imports the recorded local candidate using `r5900:LE:32:default` and runs our authored exporter. Override installations with `--ghidra-home PATH --java-home PATH`. Projects, logs, JSON, instruction listings, and candidate CSV stay in ignored `work/ghidra/`. An existing project is reopened without repeating analysis. Remove or archive it explicitly when a fresh analysis is needed.

Exports use neutral address names and remain provisional. Ghidra reports overlay and decompiler limitations. Use `prepare_map.py` for structural filtering and provenance before experimental generation; do not commit either the raw or prepared map.

## Entry control audit

```powershell
python -m unittest discover -s tools/ghidra -p "test_*.py"
python tools/ghidra/review_entry.py
```

The audit compares raw ELF control encodings with the entry export, checks instruction coverage and delay-slot inclusion, and writes ignored `work/ghidra/review/entry-control.json`. It supports only the control encodings needed by the current entry; unsupported branch families stop the audit. It verifies neither full instruction semantics nor callees, and never automatically marks a map reviewed. Static review findings are recorded in `REVERSE_ENGINEERING.md`.

After rerunning `analyze_game.py` to export the direct entry targets, run:

```powershell
python tools/ghidra/review_entry_targets.py
```

This checks the five target bodies and writes ignored `work/ghidra/review/entry-targets-control.json`. Integer branch families, including branch-likely target encodings, are supported; coprocessor branches remain unsupported. Delay-slot inclusion is checked, but slot execution/annulment is not simulated. Direct targets outside file-backed executable sections are reported for investigation, never stubbed automatically.

## Startup evidence audit

```powershell
python tools/ghidra/audit_startup.py
```

For the recorded executable hash, this independently checks three raw selector/syscall pairs and the entry's zero-loop bound constants. It reports whether `0x003B7FA8` lies in a declared load extent or the apparent zero-loop interval, plus aligned exact literal and encoded JAL matches. Output stays in ignored `work/ghidra/review/startup-evidence.json`. The scan does not resolve split constants, later writers, or executable runtime contents.

`config/startup_abi.sources.json` records the pinned PS2SDK reference revision and file hashes used for the ABI interpretation in `REVERSE_ENGINEERING.md`. These are research references, not build dependencies.

### Reached target diagnostics

`python tools/ps2recomp/inspect_blocker.py` verifies generated provenance and classifies the latest registered handler or branch target against the provisional map and Ghidra candidates. The pipeline runs it automatically after a verified stop. Read ignored `work/bt2-generation/blocker-map.json` before adding discovery targets; range membership does not certify a function boundary.

`python tools/ghidra/analyze_game.py --discover-blocker` performs focused provisional discovery at the latest observed map-gap address and re-exports candidates. It rejects an address inside an existing function and checks that the target appears in the fresh export. Then run `python tools/ps2recomp/run_pipeline.py` to regenerate from the updated automated map. Discovery is not correctness certification.

### Local kernel capture diagnostic

Capture with the installed PCSX2 and your own BIOS/game:

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin"
```

The tool copies PCSX2 and the selected BIOS into ignored `work/pcsx2-capture/`, with a separate settings profile. It instruments only a copy of the validated local ELF: 24 bytes at `0x00338388` write marker `0xB702` at `0x00387E80` and spin before the handler's first read. Read-only Windows process access captures 32 MiB of physical EE RAM from the exported `EEmem` pointer in the process it launched. It checks the marker, trap bytes, isolated data folder and stability of the kernel prefix across two reads, then terminates only that process. Hashes, instrumentation and timing are recorded in `capture.json`. Original game/BIOS files and the user's PCSX2 profile are preserved. This is an instrumented standalone ELF diagnostic; the IOP remains active during capture.

After building the pipeline, run:

```powershell
python tools/ps2recomp/probe_kernel.py work/pcsx2-capture/ee-ram.bin --capture-note "PCSX2 2.9.81; instrumented ELF handler entry, stable kernel prefix"
```

This extracts a read-only kernel window and invokes the generated scan handler in isolation. Hashes and notes are saved under ignored `work/bt2-generation/`. The result does not establish a full boot, exact BIOS exception behavior or writable kernel-patch compatibility. No capture is downloaded or bundled.

### Retail disc revision confirmation

```powershell
python tools/verify_game/verify_disc.py "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso"
```

This streams the local ISO against the official Redump size/MD5/SHA-1 recorded in `config/retail_revision.json`, reads bounded ISO9660 root records, and compares the embedded executable's size/SHA-256 with the local ELF. Reports stay under ignored `work/disc-validation/`. Only reference checksum metadata is stored in Git.

### Snapshot-driven entry pipeline

```powershell
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin
```

The snapshot requires its `.json` sidecar from `probe_kernel.py`; size, SHA-256 and capture note are checked before the run. The entry probe verifies both successful scans, their converged table base, restored stack and the next dispatch boundary. CTest retains the no-snapshot boundary. Supplying a snapshot is explicit; there is no automatic kernel-memory loading.
