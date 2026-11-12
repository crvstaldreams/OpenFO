$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$names = @('OpenFO_Diagnostics', 'startup_status.json', 'startup_error.json', 'first_frame_report.json', 'runtime_report.json', 'probe_startup_status.json', 'probe_first_frame_report.json', 'probe_runtime_report.json', 'validation_report.json', 'validation_console.txt', 'BUILD_INFO.json', 'content_catalog.json', 'openfo_settings.json','captured_scene_report.json', 'profile', 'last_exit_code.txt')
$files = @($names | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path -LiteralPath $_ })
$zip = Join-Path $root ('OpenFO_Diagnostics_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.zip')
Compress-Archive -LiteralPath $files -DestinationPath $zip -Force
Write-Host $zip
