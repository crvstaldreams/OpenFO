@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local.ps1" -Variant optimized_093 -Toolchain zig -Configuration Release -Jobs 2 -RunTests
if errorlevel 1 (
 echo.
 echo BUILD FAILED - read output\optimized_093\build.log
 pause
 exit /b 1
)
echo.
echo SUCCESS: output\optimized_093
pause
