@echo off
setlocal
where winget >nul 2>&1
if errorlevel 1 (
 echo Windows Package Manager winget is missing.
 echo Install CMake and Ninja manually from their official websites,
 echo then open a new terminal and run BUILD_LOCAL.cmd
 pause
 exit /b 1
)
winget install -e --id Kitware.CMake --accept-source-agreements --accept-package-agreements
if errorlevel 1 echo CMake install may require attention.
winget install -e --id Ninja-build.Ninja --accept-source-agreements --accept-package-agreements
if errorlevel 1 echo Ninja install may require attention.
echo.
echo Open a NEW terminal to refresh PATH, then run BUILD_LOCAL.cmd
pause
