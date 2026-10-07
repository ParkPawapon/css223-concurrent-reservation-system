@echo off
title CSS223 Cinema Theater - Box Office REPL
cd /d "%~dp0"

cls

if "%~1"=="" (
    wsl.exe --cd "%~dp0." ./build/debug/src/reservation_client 1
) else (
    wsl.exe --cd "%~dp0." ./build/debug/src/reservation_client %*
)
echo.
pause
