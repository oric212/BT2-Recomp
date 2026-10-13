@echo off
rem After the game stopped at code that is not translated yet (its message says so): double-click
rem this file. It takes the stop of the last played session, builds the missing function in and
rem rebuilds the game (a few minutes). A second window shows the progress. Then play again.
setlocal
cd /d "%~dp0"
start "BT2 discovery progress" powershell -NoProfile -Command "Get-Content '%~dp0work\bt2-generation\discovery-loop.log' -Wait -Tail 20"
python tools\ps2recomp\discovery_loop.py --player-stop
if errorlevel 1 (
    echo.
    echo Discovery did not finish. The lines above say why; the game's messages are in user\last-run.log.
) else (
    echo.
    echo Done. If the lines above say "is built in", start the game again.
    echo If they say "not a map gap", the stop is of another kind: report the end of user\last-run.log.
)
echo.
pause
