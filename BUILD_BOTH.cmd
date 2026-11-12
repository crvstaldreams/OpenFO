@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local.ps1" -Variant both -Toolchain zig -Configuration Release -Jobs 2 -RunTests
if errorlevel 1 (
 echo.
 echo BUILD FAILED - see output logs
 pause
 exit /b 1
)
echo.
echo SUCCESS: output\baseline_084 and output\optimized_093
pause
