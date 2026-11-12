param(
    [ValidateSet('optimized_093','baseline_084','both')][string]$Variant = 'optimized_093',
    [ValidateSet('Release','RelWithDebInfo','Debug')][string]$Configuration = 'Release',
    [ValidateSet('zig','msvc')][string]$Toolchain = 'zig',
    [ValidateRange(1,16)][int]$Jobs = 2,
    [string]$MasterServer = '34.118.26.31:27267',
    [switch]$RunTests
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmake = Get-Command 'cmake' -CommandType Application -ErrorAction SilentlyContinue
if (!$cmake) { throw 'CMake not found. Run INSTALL_BUILD_TOOLS.cmd and open a NEW terminal.' }
$tools = Join-Path $root '.tools'
$cache = Join-Path $root '.downloads'
New-Item -ItemType Directory -Force -Path $tools, $cache | Out-Null
$compiler = $null
$generator = 'Ninja'
if ($Toolchain -eq 'zig') {
    $ninja = Get-Command 'ninja' -CommandType Application -ErrorAction SilentlyContinue
    if (!$ninja) { throw 'Ninja not found. Run INSTALL_BUILD_TOOLS.cmd and open a NEW terminal.' }
    $zigVersion = '0.14.1'
    $zigName = "zig-x86_64-windows-$zigVersion"
    $zig = Join-Path (Join-Path $tools $zigName) 'zig.exe'
    if (!(Test-Path -LiteralPath $zig)) {
        $zip = Join-Path $cache "zig-$zigVersion.zip"
        $url = "https://ziglang.org/download/$zigVersion/$zigName.zip"
        Write-Host "Downloading compiler: $url"
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip -TimeoutSec 300
        $wanted = '554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c'
        $actual = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $wanted) { Remove-Item -LiteralPath $zip -Force; throw 'Zig download SHA256 mismatch.' }
        Expand-Archive -LiteralPath $zip -DestinationPath $tools -Force
    }
    if (!(Test-Path -LiteralPath $zig)) { throw "Zig was not unpacked: $zig" }
    $compiler = Join-Path $tools 'openfo-zig-cxx.cmd'
    $cmdSource = '@echo off' + "`r`n" + '"' + $zig + '" c++ -target x86_64-windows-gnu %*' + "`r`n" + 'exit /b %ERRORLEVEL%' + "`r`n"
    [IO.File]::WriteAllText($compiler, $cmdSource, [Text.Encoding]::ASCII)
} else {
    $generator = 'Visual Studio 17 2022'
}
function Invoke-Logged([string]$Stage, [string[]]$Arguments, [string]$LogFile) {
    Write-Host ("`n=== " + $Stage + ' ===') -ForegroundColor Cyan
    & $cmake.Source @Arguments 2>&1 | Tee-Object -FilePath $LogFile -Append
    if ($LASTEXITCODE -ne 0) { throw "$Stage failed ($LASTEXITCODE). Log: $LogFile" }
}
$variants = if ($Variant -eq 'both') { @('baseline_084','optimized_093') } else { @($Variant) }
foreach ($name in $variants) {
    $src = Join-Path $root (Join-Path 'source' $name)
    $build = Join-Path $root (Join-Path 'build' (Join-Path $Toolchain $name))
    $out = Join-Path $root (Join-Path 'output' $name)
    New-Item -ItemType Directory -Force -Path $build, $out | Out-Null
    $log = Join-Path $out 'build.log'
    ('OpenFO local build | variant=' + $name + ' | config=' + $Configuration + ' | toolchain=' + $Toolchain + ' | ' + (Get-Date -Format 's')) | Set-Content -LiteralPath $log -Encoding UTF8
    & (Join-Path $PSScriptRoot 'fetch-deps.ps1') -SourceDir $src -CacheDir $cache
    $configArgs = @('-S',$src,'-B',$build,'-G',$generator,"-DOPENFO_MASTER_SERVER=$MasterServer")
    if ($Toolchain -eq 'zig') {
        $configArgs += "-DCMAKE_CXX_COMPILER=$compiler"
        $configArgs += "-DCMAKE_MAKE_PROGRAM=$($ninja.Source)"
        $configArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
        $configArgs += '-DCMAKE_EXE_LINKER_FLAGS=-static'
    } else { $configArgs += @('-A','x64') }
    Invoke-Logged "Configure $name" $configArgs $log
    $targets = @('OpenFO','OpenFOD3D9','OpenFO_Diagnostics_OpenGL','OpenFO_Diagnostics_D3D9','OpenFOValidate')
    $buildArgs = @('--build',$build,'--config',$Configuration,'--parallel',"$Jobs",'--target') + $targets
    Invoke-Logged "Build $name" $buildArgs $log
    $exes = @('OpenFO.exe','OpenFO_D3D9.exe','OpenFO_Diagnostics_OpenGL.exe','OpenFO_Diagnostics_D3D9.exe','OpenFOValidate.exe')
    $configPath = if ($Toolchain -eq 'msvc') { Join-Path $build $Configuration } else { $build }
    foreach ($exe in $exes) {
        $from = Join-Path $configPath $exe
        if (!(Test-Path -LiteralPath $from)) { throw "Build reported success but missing $from" }
        Copy-Item -LiteralPath $from -Destination (Join-Path $out $exe) -Force
        $hash = (Get-FileHash -LiteralPath (Join-Path $out $exe) -Algorithm SHA256).Hash
        Add-Content -LiteralPath (Join-Path $out 'SHA256SUMS.txt') -Value "$hash  $exe"
    }
    $scriptsDir = Join-Path $src 'openfo_scripts'
    if (Test-Path -LiteralPath $scriptsDir) { Copy-Item -LiteralPath $scriptsDir -Destination $out -Recurse -Force }
    if ($RunTests) {
        $testTargets = if ($name -eq 'baseline_084') { @('test_clock') } else { @('test_clock','test_cpu_parallel','test_display_resolution090','test_vehicle_camera_ini_092') }
        Invoke-Logged "Build unit tests $name" (@('--build',$build,'--config',$Configuration,'--parallel',"$Jobs",'--target') + $testTargets) $log
        $ctest = Get-Command 'ctest' -CommandType Application -ErrorAction SilentlyContinue
        if (!$ctest) { throw 'ctest not found alongside CMake.' }
        $testPattern = if ($name -eq 'baseline_084') {'^frame_clock$'} else {'^(frame_clock|cpu_parallel|display_resolution090|vehicle_camera_ini_092)$'}
        & $ctest.Source --test-dir $build -C $Configuration --output-on-failure -R $testPattern 2>&1 | Tee-Object -FilePath $log -Append
        if ($LASTEXITCODE -ne 0) { throw "Tests failed; see $log" }
    }
    Write-Host "Build complete: $out" -ForegroundColor Green
}
Write-Host '`nCopy built EXEs into a COPY of your game directory containing original data/, not into the source directory.' -ForegroundColor Yellow
