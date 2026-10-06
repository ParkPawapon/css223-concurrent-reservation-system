@echo off
title CSS223 Cinema Theater - Box Office REPL
cd /d "%~dp0"

cls

wsl.exe --cd "%~dp0." ./build/debug/src/reservation_client
echo.
pause
