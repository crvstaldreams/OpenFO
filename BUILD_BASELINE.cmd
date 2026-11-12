@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local.ps1" -Variant baseline_084 -Toolchain zig -Configuration Release -Jobs 2 -RunTests
if errorlevel 1 (
 echo.
 echo BUILD FAILED - read output\baseline_084\build.log
 pause
 exit /b 1
)
echo.
echo SUCCESS: output\baseline_084
pause
