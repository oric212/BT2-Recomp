# BT2 Native Recompilation Project

A native Windows x64 port of Dragon Ball Z: Budokai Tenkaichi 2 using static recompilation of the original PS2 game logic and a custom PS2 compatibility runtime. The initial target is the North American release, SLUS-21441. This is a recompilation project, not a ground-up remake.

## Current status

The C++20 host executable `bt2-port` builds with CMake and MSVC from Visual Studio 2022 and prints a startup message. A read-only local executable validator performs preliminary format, filename, boot reference, and SHA-256 checks. Exact retail revision confirmation, game loading, recompilation, rendering, and audio remain pending.

Game files are not included. Users supply files from their own copy locally under ignored `game_data/` or `rom/`. Never commit proprietary content or generated code derived from the retail executable.

See [build instructions](BUILDING.md), [project goals](PROJECT_GOALS.md), [progress](../PROG.md), [content and license rules](LEGAL.md), [local validation](VERIFY_GAME.md), and [reverse-engineering notes](REVERSE_ENGINEERING.md).

## Layout

- `runtime/`: native host and future PS2 compatibility code.
- `game/`: handwritten BT2 integration; proprietary-derived output belongs in ignored `game/generated/`.
- `config/`: shared configuration examples when needed.
- `tools/`: development tools, including preliminary local executable verification.
- `external/`: dependency integration notes; no dependencies are bundled yet.
- `docs/`: goals, build instructions, progress, and evidence records.

No configuration, game integration, or external dependencies are implemented yet. Add shared configuration examples only when needed; record dependency source, pinned version, license, and build requirements before integration. Keep local game paths under ignored `game_data/` or `rom/`.

See [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) for the ordered plan and [PROG.md](../PROG.md) for verified progress.
