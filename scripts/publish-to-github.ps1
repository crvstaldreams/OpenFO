param(
    [ValidatePattern('^[A-Za-z0-9_.-]+$')][string]$Repository = 'OpenFO-Windows-Build',
    [ValidateSet('private','public')][string]$Visibility = 'private'
)
$ErrorActionPreference = 'Stop'
Set-Location (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
function Require-Tool([string]$Name) {
    if (!(Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue)) {
        throw "Missing $Name. Run INSTALL_GITHUB_TOOLS.cmd, restart terminal."
    }
}
Require-Tool 'gh'
Require-Tool 'git'
& gh auth status
if ($LASTEXITCODE -ne 0) { throw 'Not logged in. Run: gh auth login --web' }
$login = (& gh api user --jq .login).Trim()
if ($LASTEXITCODE -ne 0 -or !$login) { throw 'Unable to get GitHub login.' }
$remoteName = "$login/$Repository"
if (Test-Path '.git') {
    throw 'Folder already has a Git repository. Review it manually to avoid overwriting commits.'
}
Write-Host "Publishing as $remoteName ($Visibility)." -ForegroundColor Cyan
Write-Host 'No original game assets are included.' -ForegroundColor Yellow
& git init -b main
if ($LASTEXITCODE -ne 0) { throw 'git init failed' }
& git config user.name $login
& git config user.email "$login@users.noreply.github.com"
& git add -A
if ($LASTEXITCODE -ne 0) { throw 'git add failed' }
& git commit -m 'Initial OpenFO source + Windows x64 GitHub Actions build'
if ($LASTEXITCODE -ne 0) { throw 'git commit failed' }
if ($Visibility -eq 'private') {
    & gh repo create $remoteName --private --source . --remote origin --push
} else {
    & gh repo create $remoteName --public --source . --remote origin --push
}
if ($LASTEXITCODE -ne 0) { throw 'GitHub repository creation / push failed. Check gh auth and whether repository already exists.' }
Write-Host "Repository: https://github.com/$remoteName" -ForegroundColor Green
Write-Host "Actions: https://github.com/$remoteName/actions/workflows/windows-build.yml" -ForegroundColor Green
