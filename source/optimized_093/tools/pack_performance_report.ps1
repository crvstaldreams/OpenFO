[CmdletBinding()]
param(
    [string[]]$Session,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = $root }
$diagnostics = Join-Path $root 'OpenFO_Diagnostics'

function Read-JsonFile([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        try { return (Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json) }
        catch { Write-Verbose ('Skipping unreadable JSON ' + $Path) }
    }
}

function Get-RunReport([System.IO.DirectoryInfo]$Folder, $System) {
    $samplePath = Join-Path $Folder.FullName 'samples.jsonl'
    $last = $null
    $raceWindows = 0
    $raceTime = 0.0
    $raceFrames = 0
    $minFps = [double]::PositiveInfinity
    $maxFps = 0.0
    if (Test-Path -LiteralPath $samplePath -PathType Leaf) {
        Get-Content -LiteralPath $samplePath -Encoding UTF8 | ForEach-Object {
            try {
                $row = $_ | ConvertFrom-Json
                if ($null -ne $row) {
                    $last = $row
                    if ($row.race_only -eq $true -and $row.page -eq 'race' -and [double]$row.sample_ms -gt 0) {
                        ++$raceWindows
                        $raceTime += [double]$row.sample_ms
                        $raceFrames += [long]$row.frames_presented
                        $minFps = [Math]::Min($minFps, [double]$row.fps)
                        $maxFps = [Math]::Max($maxFps, [double]$row.fps)
                    }
                }
            } catch {
                Write-Verbose 'Skipping incomplete sample line'
            }
        }
    }
    $race = $null
    if ($raceWindows -gt 0) {
        $race = [ordered]@{
            race_only_windows = $raceWindows
            duration_ms = [Math]::Round($raceTime, 1)
            presented_frames = $raceFrames
            average_fps = [Math]::Round(1000.0 * $raceFrames / $raceTime, 2)
            minimum_sample_fps = [Math]::Round($minFps, 2)
            maximum_sample_fps = [Math]::Round($maxFps, 2)
        }
    }
    $screenshots = @()
    $snapshots = Join-Path $Folder.FullName 'snapshots'
    if (Test-Path -LiteralPath $snapshots -PathType Container) {
        $screenshots = @(Get-ChildItem -LiteralPath $snapshots -Filter 'drop_*.png' -File)
    }
    return [ordered]@{
        session = $Folder.Name
        backend = $System.backend
        started_utc = $System.session_started_utc
        cpu = $System.host.cpu.model
        ram_total_bytes = $System.host.ram_total_bytes
        gpu = $System.active_graphics_adapter.name
        graphics_last_sample = $last.graphics
        race_fps = $race
        screenshot_count = $screenshots.Count
        session_summary = Read-JsonFile (Join-Path $Folder.FullName 'summary.json')
        directory_in_zip = ('sessions/' + $Folder.Name)
    }
}

if ($Session) {
    $runs = @(foreach ($path in $Session) {
        $folder = Get-Item -LiteralPath $path
        if (!$folder.PSIsContainer) { throw ('A session directory is required: ' + $path) }
        $folder
    })
} else {
    if (!(Test-Path -LiteralPath $diagnostics -PathType Container)) {
        throw 'Run an OpenFO diagnostics EXE first.'
    }
    $candidates = @(Get-ChildItem -LiteralPath $diagnostics -Directory | Where-Object {
        $_.Name -match '^(OpenGL|D3D9Ex)_\d{8}T\d{6}Z_\d+$' -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'system.json') -PathType Leaf)
    })
    $runs = @(foreach ($backend in @('OpenGL', 'D3D9Ex')) {
        $candidates | Where-Object { $_.Name.StartsWith($backend + '_') } |
            Sort-Object Name -Descending | Select-Object -First 1
    })
}
$runs = @($runs | Sort-Object FullName -Unique)
if (!$runs.Count) { throw 'No OpenFO performance sessions found.' }

$entries = @()
foreach ($run in $runs) {
    $system = Read-JsonFile (Join-Path $run.FullName 'system.json')
    if (!$system -or $system.schema -ne 1 -or $system.backend -notin @('OpenGL', 'D3D9Ex')) {
        throw ('Invalid OpenFO performance session: ' + $run.FullName)
    }
    if ($run.Name -notmatch '^[a-zA-Z0-9_-]+$') { throw ('Invalid session directory name: ' + $run.Name) }
    $entries += (Get-RunReport $run $system)
}
if (@($runs | Group-Object Name | Where-Object Count -gt 1).Count) {
    throw 'Session directory names must be distinct.'
}
$report = [ordered]@{
    schema = 1
    kind = 'openfo_performance_report'
    collected_utc = [DateTime]::UtcNow.ToString('o')
    sessions = $entries
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$stamp = [DateTime]::UtcNow.ToString("yyyyMMdd_HHmmss_fff'Z'")
$zipPath = Join-Path $OutputDirectory ('OpenFO_Performance_' + $stamp + '.zip')
$temp = $zipPath + '.' + [Guid]::NewGuid().ToString('N') + '.partial'
try {
    $file = [System.IO.File]::Open($temp, [System.IO.FileMode]::CreateNew)
    try {
        $zip = [System.IO.Compression.ZipArchive]::new($file, [System.IO.Compression.ZipArchiveMode]::Create, $true)
        try {
            $entry = $zip.CreateEntry('report.json')
            $writer = [System.IO.StreamWriter]::new($entry.Open(), [System.Text.UTF8Encoding]::new($false))
            try { $writer.Write(($report | ConvertTo-Json -Depth 14)) }
            finally { $writer.Dispose() }

            $names = @('system.json', 'summary.json', 'frames.csv', 'samples.jsonl', 'stutters.jsonl',
                       'runtime_report.json', 'startup_status.json', 'startup_error.json', 'first_frame_report.json',
                       'probe_runtime_report.json', 'probe_startup_status.json', 'probe_first_frame_report.json',
                       'OpenFO_Diagnostics/crash_report.txt')
            foreach ($run in $runs) {
                $prefix = 'sessions/' + $run.Name + '/'
                foreach ($name in $names) {
                    $source = Join-Path $run.FullName $name
                    if (Test-Path -LiteralPath $source -PathType Leaf) {
                        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                            $zip, $source, ($prefix + $name), [System.IO.Compression.CompressionLevel]::Optimal)
                    }
                }
                $snapshots = Join-Path $run.FullName 'snapshots'
                if (Test-Path -LiteralPath $snapshots -PathType Container) {
                    Get-ChildItem -LiteralPath $snapshots -File | Where-Object { $_.Name -match '^drop_\d+\.(json|png)$' } |
                        ForEach-Object {
                            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                                $zip, $_.FullName, ($prefix + 'snapshots/' + $_.Name),
                                [System.IO.Compression.CompressionLevel]::Optimal)
                        }
                }
            }
        } finally { $zip.Dispose() }
    } finally { $file.Dispose() }
    [System.IO.File]::Move($temp, $zipPath)
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
}
Write-Output $zipPath
