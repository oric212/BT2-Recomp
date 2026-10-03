# Building on Windows

## Requirements

- Windows 10 or 11 x64.
- Visual Studio 2022 with Desktop development with C++, MSVC, and a Windows SDK.
- CMake 3.20 or newer, available in the terminal.

## Build and run

From PowerShell:

```powershell
cd 'D:\Cs\C++\BT2-Recomp'
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\bt2-port.exe
```

For an explicit Visual Studio 2022 x64 selection in a fresh build directory, configure with:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Expected output:

```text
BT2 Native Recompilation Project
No game data loaded.
```

No game files or external dependencies are required for this foundation. Build output stays under ignored `build/`.
