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
