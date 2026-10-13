@echo off
rem Starts the launcher of the Release build.
cd /d "%~dp0"
if not exist build\probe\Release\bt2-launcher.exe (
    echo The game is not built yet: build\probe\Release\bt2-launcher.exe is missing.
    pause
    exit /b 1
)
start "" build\probe\Release\bt2-launcher.exe
