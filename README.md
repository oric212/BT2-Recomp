# BT2 Native Recompilation Project

A native Windows x64 port of Dragon Ball Z: Budokai Tenkaichi 2 using static recompilation of the original PS2 game logic and a custom PS2 compatibility runtime. The initial target is the North American release, SLUS-21441. This is a recompilation project, not a ground-up remake.

## Current status

The C++20 host executable `bt2-port` builds with CMake and MSVC from Visual Studio 2022 and prints a startup message. Game loading, validation, recompilation, rendering, and audio are not implemented.

Game files are not included. Users supply files from their own copy locally under ignored `game_data/` or `rom/`. Never commit proprietary content or generated code derived from the retail executable.

See [build instructions](docs/BUILDING.md), [project goals](docs/PROJECT_GOALS.md), [progress](docs/PROGRESS.md), [content and license rules](docs/LEGAL.md), and [reverse-engineering notes](docs/REVERSE_ENGINEERING.md).

## Layout

- `runtime/`: native host and future PS2 compatibility code.
- `game/`: handwritten BT2 integration; proprietary-derived output belongs in ignored `game/generated/`.
- `config/`: shared configuration examples when needed.
- `tools/`: development tools; game verification is only a documented next step.
- `external/`: dependency integration notes; no dependencies are bundled yet.
- `docs/`: goals, build instructions, progress, and evidence records.
