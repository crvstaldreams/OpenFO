param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$CacheDir
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$vendor = Join-Path $SourceDir 'vendor'
New-Item -ItemType Directory -Force -Path $CacheDir, $vendor | Out-Null

function Get-FileOnce([string]$Uri, [string]$File) {
    if (!(Test-Path -LiteralPath $File) -or (Get-Item -LiteralPath $File).Length -lt 1024) {
        Write-Host "Downloading $Uri"
        $partial = "$File.partial"
        if (Test-Path $partial) { Remove-Item -Force $partial }
        try { Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $partial -TimeoutSec 300 }
        catch { if (Test-Path $partial) {Remove-Item -Force $partial}; throw }
        Move-Item -LiteralPath $partial -Destination $File -Force
    }
    Write-Host ('SHA256 {0}  {1}' -f (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash, (Split-Path -Leaf $File))
}

function Install-ZippedSources([string]$Url, [string]$ArchiveName, [string]$Relative, [string]$Target) {
    $archive = Join-Path $CacheDir $ArchiveName
    Get-FileOnce $Url $archive
    $unpack = Join-Path $CacheDir ($ArchiveName + '.extracted')
    if (!(Test-Path -LiteralPath $unpack)) {
        Expand-Archive -LiteralPath $archive -DestinationPath $unpack -Force
    }
    $rootFolders = @(Get-ChildItem -LiteralPath $unpack -Directory)
    if ($rootFolders.Count -ne 1) { throw "Unexpected archive structure: $archive" }
    $src = Join-Path $rootFolders[0].FullName $Relative
    if (!(Test-Path -LiteralPath $src)) { throw "Missing $Relative in $archive" }
    New-Item -ItemType Directory -Force -Path $Target | Out-Null
    # Copy all upstream source files and directories, preserving hierarchy.
    foreach ($f in (Get-ChildItem -LiteralPath $src -Force)) {
        Copy-Item -LiteralPath $f.FullName -Destination $Target -Recurse -Force
    }
    $license = @('LICENSE','LICENSE.md','LICENSE.txt','LICENSE.TXT') | ForEach-Object { Join-Path $rootFolders[0].FullName $_ } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($license) { Copy-Item -LiteralPath $license -Destination (Join-Path $Target 'UPSTREAM_LICENSE.txt') -Force }
}

$bullet = Join-Path $vendor 'bullet/src'
if (!(Test-Path (Join-Path $bullet 'btBulletDynamicsCommon.h'))) {
    Install-ZippedSources 'https://github.com/bulletphysics/bullet3/archive/refs/tags/3.25.zip' 'bullet3-3.25.zip' 'src' $bullet
}
$mesh = Join-Path $vendor 'meshoptimizer'
if (!(Test-Path (Join-Path $mesh 'simplifier.cpp')) -or !(Test-Path (Join-Path $mesh 'meshoptimizer.h'))) {
    Install-ZippedSources 'https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.25.zip' 'meshoptimizer-v0.25.zip' 'src' $mesh
}
$stbOut = Join-Path $vendor 'stb_vorbis.cpp'
if (!(Test-Path -LiteralPath $stbOut)) {
    $stb = Join-Path $CacheDir 'stb_vorbis-pinned.c'
    Get-FileOnce 'https://raw.githubusercontent.com/nothings/stb/9f755b604db3492df2cbeb39bf84b81e3e3932e3/stb_vorbis.c' $stb
    Copy-Item -LiteralPath $stb -Destination $stbOut
}
# Windows SDK ships legacy GL/gl.h but does not ship the OpenGL extension header.
# Pin Khronos header snapshots for deterministic MSVC builds.
$glDir = Join-Path $vendor 'GL'
$khrDir = Join-Path $vendor 'KHR'
New-Item -ItemType Directory -Force -Path $glDir, $khrDir | Out-Null
$glExt = Join-Path $glDir 'glext.h'
if (!(Test-Path -LiteralPath $glExt)) {
    Get-FileOnce 'https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/6af574a14089ccfee87efe230ebcdd8742859813/api/GL/glext.h' $glExt
}
$khrPlatform = Join-Path $khrDir 'khrplatform.h'
if (!(Test-Path -LiteralPath $khrPlatform)) {
    Get-FileOnce 'https://raw.githubusercontent.com/KhronosGroup/EGL-Registry/8e56e86e65687697ef54114cdb1afabdfac756bf/api/KHR/khrplatform.h' $khrPlatform
}
$required = @(
    (Join-Path $bullet 'btBulletDynamicsCommon.h'),
    (Join-Path $bullet 'BulletDynamics/Dynamics/btRigidBody.cpp'),
    (Join-Path $mesh 'meshoptimizer.h'),
    (Join-Path $mesh 'simplifier.cpp'),
    (Join-Path $mesh 'allocator.cpp'),
    $glExt,
    $khrPlatform,
    $stbOut,
    (Join-Path $vendor 'nlohmann/json.hpp')
)
foreach ($file in $required) { if (!(Test-Path -LiteralPath $file)) {throw "Missing vendor dependency: $file"} }
if (!(Select-String -LiteralPath (Join-Path $mesh 'meshoptimizer.h') -Pattern 'meshopt_SimplifyPermissive' -Quiet)) { throw 'meshoptimizer API too old: SimplifyPermissive missing.' }
Write-Host 'Vendor dependencies are ready.' -ForegroundColor Green
