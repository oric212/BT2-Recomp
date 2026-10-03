# Local game verification

Requires Python 3.10 or newer; uses only the standard library. From the repository root:

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41
```

The tool reads the local executable without changing it and prints JSON containing size, SHA-256, and preliminary validation results. It checks the filename and basic ELF32 little-endian MIPS executable header. A sibling `SYSTEM.CNF`, when present, must name `SLUS_214.41` in its boot reference. Use `--system-cnf PATH` to supply it explicitly.

Use `--expected-sha256 HASH` to compare against an independently trusted reference. Without that reference, successful preliminary checks do not authenticate the retail revision. This tool does not parse ELF tables, inspect functions, or load the game. Failures return a nonzero exit code.

Keep game inputs under ignored `rom/` or `game_data/` and temporary reports under ignored `work/`.
