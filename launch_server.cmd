@echo off
title CSS223 Cinema Reservation Server - Multi-Worker Core
cd /d "%~dp0"

cls

if "%~1"=="" (
    wsl.exe --cd "%~dp0." ./build/debug/src/reservation_server --workers 5 --delay
) else (
    wsl.exe --cd "%~dp0." ./build/debug/src/reservation_server %*
)
echo.
pause
