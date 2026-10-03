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

## Local BT2 generation (awaiting reviewed map)

Review function boundaries in the local executable using Ghidra and the pinned upstream `ps2xRecomp/tools/ghidra/ExportPS2Functions.java`. Keep projects and maps under ignored `work/` or `research/`. Preserve neutral `FUN_<eight-digit-address>` names until their purpose is established. The initial driver accepts contiguous aligned ranges within executable sections; noncontiguous bodies require review and future support.

```powershell
python tools/ps2recomp/generate_game.py --function-map work/bt2-functions.csv
```

CSV columns are `Name,Start,End,Size` (case insensitive), with exclusive ends. The driver checks the locally recorded candidate size/hash, map ranges, and tool provenance. Matching the candidate hash establishes consistency with our inspected file, not authenticity against a retail reference. It writes local configuration under `work/bt2-generation/` and C++ under ignored `game/generated/bt2/`, with automatic instruction patches disabled and no manual stubs or skips.

These checks cannot certify map correctness. Generated output can include upstream heuristic discoveries; treat them as candidates. BT2 generation has not run because no reviewed map exists. The emitted code depends on upstream runtime APIs, so custom runtime adaptation remains future work.

## Installed Ghidra analysis

Project root: `D:\Cs\Cpp\BT2-Recomp`. Ghidra 12.1.4 is installed at `D:\Cs\Cpp\tools\ghidra_12.1.4_PUBLIC`; launch the GUI with `D:\Cs\Cpp\tools\run-ghidra.bat`. It uses the existing Adoptium JDK 25. Tool versions and archive hashes are recorded in `config/ghidra.lock.json`.

The R5900 extension v2.1.37 was built from pinned source against this Ghidra version. Gradle 9.8.0 is installed beside Ghidra. These tools remain outside the repository.

```powershell
python tools/ghidra/analyze_game.py
```

The driver imports the recorded local candidate using `r5900:LE:32:default` and runs our authored exporter. Override installations with `--ghidra-home PATH --java-home PATH`. Projects, logs, JSON, instruction listings, and candidate CSV stay in ignored `work/ghidra/`. An existing project is reopened without repeating analysis. Remove or archive it explicitly when a fresh analysis is needed.

Exports use neutral address names and remain unreviewed. Ghidra reports overlay and decompiler limitations; verify boundaries and control flow before preparing a reviewed map. Do not feed the candidate CSV directly into game generation or commit it.
