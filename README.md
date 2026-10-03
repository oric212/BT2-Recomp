# BT2 Native Recompilation Project

A native Windows x64 port of Dragon Ball Z: Budokai Tenkaichi 2 using static recompilation of the original PS2 game logic and a custom PS2 compatibility runtime. The initial target is the North American release, SLUS-21441. This is a recompilation project, not a ground-up remake.

## Current status

The C++20 host executable `bt2-port` builds with CMake and MSVC from Visual Studio 2022 and prints a startup message. Read-only tools validate the local executable preliminarily and inspect ELF metadata. The declared entry point and load regions are recorded with an alignment warning. Exact retail revision confirmation, game loading, recompilation, rendering, and audio remain pending.

Game files are not included. Users supply files from their own copy locally under ignored `game_data/` or `rom/`. Never commit proprietary content or generated code derived from the retail executable.

See [build instructions](docs/BUILDING.md), [the implementation plan](docs/IMPLEMENTATION_PLAN.md), [progress](docs/PROG.md), [content and license rules](docs/LEGAL.md), [local tools](docs/TOOLS.md), and [reverse-engineering notes](docs/REVERSE_ENGINEERING.md).

## Layout

- `runtime/`: native host and future PS2 compatibility code.
- `game/`: handwritten BT2 integration; proprietary-derived output belongs in ignored `game/generated/`.
- `config/`: shared configuration examples when needed.
- `tools/`: development tools, including preliminary local executable verification.
- `external/`: dependency integration notes; no dependencies are bundled yet.
- `docs/`: goals, build instructions, progress, and evidence records.

Pinned PS2Recomp tools build separately from the host and pass synthetic translation. Dependency checkouts stay under ignored `work/`; versions are in `config/ps2recomp.lock.json`. BT2 generation requires a reviewed function map, and custom runtime integration is pending. Keep local game paths under ignored `game_data/` or `rom/`.

Ghidra with R5900 support is installed under `D:\Cs\Cpp\tools`. Local analysis exports unreviewed function candidates under ignored `work/ghidra/`; boundary review remains pending.
