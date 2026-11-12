@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\pack_performance_report.ps1" %*
set "result=%ERRORLEVEL%"
if not "%result%"=="0" echo OpenFO report packaging failed.
pause
exit /b %result%
