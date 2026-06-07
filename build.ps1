# Build LEAP Protocol (Windows / MSVC + CMake)
#
# Output: local build-win/ (gitignored). See docs/BUILD.md.
#
# Usage:
#   .\build.ps1                    # incremental Release build (Studio + CLI + tests)
#   .\build.ps1 -Configure         # re-run CMake configure first
#   .\build.ps1 -Configuration Debug
#   .\build.ps1 -Target leap_win_controller
#   .\build.ps1 -Test              # run leap_tests after build
#   .\build.ps1 -Clean             # delete build-win and configure from scratch
#
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-win",
    [string]$QtRoot = "C:\Qt\6.8.3\msvc2022_64",
    [string[]]$Target = @(
        "leap_studio_qt",
        "leap_conformance",
        "leap_tests",
        "leap_win_controller",
        "leap_win_device",
        "leap_win_discover",
        "leap_win_identify",
        "leap_win_hub"
    ),
    [switch]$Configure,
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Allow: -Target leap_tests -Target leap_studio_qt  OR  -Target leap_tests,leap_conformance
if ($Target.Count -eq 1 -and $Target[0] -match ",") {
    $Target = $Target[0].Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ }
}

function Find-CMake {
    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw "cmake.exe not found. Install Visual Studio 2022 with the C++ CMake tools workload."
}

$RepoRoot = $PSScriptRoot
$BuildPath = Join-Path $RepoRoot $BuildDir
$Cmake = Find-CMake

if ($Clean -and (Test-Path $BuildPath)) {
    Write-Host "Removing $BuildPath ..."
    Remove-Item -LiteralPath $BuildPath -Recurse -Force
}

$cacheFile = Join-Path $BuildPath "CMakeCache.txt"
$needsConfigure = $Configure -or -not (Test-Path $cacheFile)
if (-not $needsConfigure -and (Test-Path $cacheFile)) {
    $cacheText = Get-Content -LiteralPath $cacheFile -Raw
    if ($cacheText -match 'Qt6_DIR:PATH=Qt6_DIR-NOTFOUND') {
        Write-Host "CMake cache missing Qt6 - reconfiguring ..."
        $needsConfigure = $true
    }
}

if ($needsConfigure) {
    Write-Host "Configuring ($Configuration) -> $BuildPath"
    if (-not (Test-Path -LiteralPath $QtRoot)) {
        throw "Qt not found at $QtRoot. Install Qt 6.8.x msvc2022_64 or pass -QtRoot."
    }
    & $Cmake -S $RepoRoot -B $BuildPath `
        -DLEAP_BUILD_WIN_L2=ON `
        -DLEAP_BUILD_STUDIO_QT=ON `
        "-DCMAKE_PREFIX_PATH=$QtRoot"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Building $Configuration ..."
foreach ($name in $Target) {
    Write-Host "  -> $name"
    & $Cmake --build $BuildPath --config $Configuration --target $name
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$StudioExe = Join-Path $BuildPath "leap_studio_qt\Release\leap_studio_qt.exe"
if ($Configuration -ne "Release") {
    $StudioExe = Join-Path $BuildPath "leap_studio_qt\$Configuration\leap_studio_qt.exe"
}
$ConformanceExe = Join-Path $BuildPath "leap_cli\$Configuration\leap_conformance.exe"
$TestsExe = Join-Path $BuildPath "$Configuration\leap_tests.exe"

Write-Host ""
Write-Host "Build OK ($Configuration)"
if (Test-Path $StudioExe) {
    Write-Host "  Studio:      $StudioExe"
}
if (Test-Path $ConformanceExe) {
    Write-Host "  Conformance: $ConformanceExe"
}
if (Test-Path $TestsExe) {
    Write-Host "  Tests:       $TestsExe"
}

if ($Test) {
    if (-not (Test-Path $TestsExe)) {
        throw "leap_tests.exe not found at $TestsExe"
    }
    Write-Host ""
    Write-Host "Running leap_tests ..."
    & $TestsExe
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
