@echo off
title CSS223 Cinema Theater - Box Office REPL
cd /d "%~dp0"

for /f "tokens=2 delims=:" %%a in ('mode con ^| findstr /c:"Columns"') do (
    set /a COLS=%%a
)
if not defined COLS set COLS=120

set COLUMNS=%COLS%
set WSLENV=COLUMNS/u

cls

wsl.exe --cd "%~dp0." ./build/debug/src/reservation_client
echo.
pause
