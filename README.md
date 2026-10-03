# BT2 Native Recompilation Project

A native Windows x64 port of Dragon Ball Z: Budokai Tenkaichi 2 using static recompilation of the original PS2 game logic and a custom PS2 compatibility runtime. The initial target is the North American release, SLUS-21441. This is a recompilation project, not a ground-up remake.

## Current status

The C++20 host executable `bt2-port` builds with CMake and MSVC from Visual Studio 2022. Automated mapping and PS2Recomp generation work locally. Optional probes execute a generated BT2 syscall wrapper and generated entry CPU initialization/RAM clearing. Entry stops at unsupported SetupThread. Game boot, full runtime integration, rendering and audio remain pending; exact retail revision authentication still needs a trusted reference.

Game files are not included. Users supply files from their own copy locally under ignored `game_data/` or `rom/`. Never commit proprietary content or generated code derived from the retail executable.

See [build instructions](docs/BUILDING.md), [the implementation plan](docs/IMPLEMENTATION_PLAN.md), [progress](docs/PROG.md), [content and license rules](docs/LEGAL.md), [local tools](docs/TOOLS.md), and [reverse-engineering notes](docs/REVERSE_ENGINEERING.md).

## Layout

- `runtime/`: native host and future PS2 compatibility code.
- `game/`: handwritten BT2 integration; proprietary-derived output belongs in ignored `game/generated/`.
- `config/`: shared configuration examples when needed.
- `tools/`: development tools, including preliminary local executable verification.
- `external/`: dependency integration notes; no dependencies are bundled yet.
- `docs/`: goals, build instructions, progress, and evidence records.

Pinned PS2Recomp tools build separately from the host. Dependency checkouts stay under ignored `work/`; versions are in `config/ps2recomp.lock.json`. The workflow uses a provisional neutral function map and advances through generation, builds, execution and blocker diagnostics. Keep local game paths under ignored `game_data/` or `rom/`.

Ghidra with R5900 support is installed under `D:\Cs\Cpp\tools`. Local analysis exports unreviewed function candidates under ignored `work/ghidra/`; boundary review remains pending.

Run `python tools/ps2recomp/run_pipeline.py` after local Ghidra export and tool setup; use `--reuse-generated` to verify and reuse generated output. See [reference architecture](docs/REFERENCE_ARCHITECTURE.md). Passing probes establish execution to checked boundaries; the game has not booted.
