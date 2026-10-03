# BT2 Recomp

A clean C++ foundation for a future Budokai Tenkaichi 2 recompilation effort. The current executable is a startup smoke check; it does not load or execute game code yet.

## Build and run

Requires CMake 3.20 or newer and a C++17 compiler.

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\bt2-recomp.exe
```

Run `bt2-recomp --help` for the available command line options.

## Local data

Keep legally obtained game data and reverse-engineering work outside version control. The existing `rom/` directory, `game_data/`, retail executables, generated code, build output, and scratch files are ignored. Do not add proprietary assets or code copied from the retail executable to this repository.
