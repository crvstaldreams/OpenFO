@echo off
echo Installing Git and GitHub CLI (winget)...
winget install --id Git.Git -e --accept-source-agreements --accept-package-agreements
winget install --id GitHub.cli -e --accept-source-agreements --accept-package-agreements
echo Restart the terminal, then run gh auth login.
pause
