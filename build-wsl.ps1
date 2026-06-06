# Build LEAP Protocol in WSL (Linux GCC + CMake)
#
# Use this from Windows PowerShell to compile and test the Linux tree the same
# way GitHub Actions does. Output: local build-wsl/ (gitignored). See docs/BUILD.md.
#
# Usage:
#   .\build-wsl.ps1                 # incremental Release build
#   .\build-wsl.ps1 -Configure      # re-run CMake configure first
#   .\build-wsl.ps1 -Test           # build + run ctest
#   .\build-wsl.ps1 -Clean          # delete build-wsl and configure from scratch
#   .\build-wsl.ps1 -Target leap_tests
#
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-wsl",
    [string]$WslDistro = "",
    [string[]]$Target = @("leap_tests"),
    [switch]$Configure,
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

if ($Target.Count -eq 1 -and $Target[0] -match ",") {
    $Target = $Target[0].Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ }
}

function Convert-ToWslPath {
    param([string]$Path)

    $full = [System.IO.Path]::GetFullPath($Path)
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLower()
        $rest  = ($Matches[2] -replace '\\', '/')
        return "/mnt/$drive/$rest"
    }

    return ($full -replace '\\', '/')
}

function Invoke-WslBash {
    param([string]$Script)

    $wslArgs = @()
    if ($WslDistro -ne "") {
        $wslArgs += @("-d", $WslDistro)
    }
    $wslArgs += @("bash", "-s")

    $Script | & wsl @wslArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$wslCmd = Get-Command wsl -ErrorAction SilentlyContinue
if (-not $wslCmd) {
    throw "wsl.exe not found. Install WSL (Ubuntu) and retry."
}

$RepoRoot = $PSScriptRoot
$WslRepo  = Convert-ToWslPath $RepoRoot
$WslBuild = Convert-ToWslPath (Join-Path $RepoRoot $BuildDir)
$targetCsv = ($Target -join " ")

$configureFlag = if ($Configure) { "yes" } else { "no" }
$cleanFlag     = if ($Clean) { "yes" } else { "no" }

$buildScript = @'
set -euo pipefail
REPO="__REPO__"
BUILD="__BUILD__"
CONFIG="__CONFIG__"
CONFIGURE="__CONFIGURE__"
CLEAN="__CLEAN__"
TARGETS="__TARGETS__"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found in WSL. Install build tools:" >&2
  echo "  sudo apt update && sudo apt install -y build-essential cmake" >&2
  exit 1
fi

cd "$REPO"

if [ "$CLEAN" = "yes" ] && [ -d "$BUILD" ]; then
  echo "Removing $BUILD ..."
  rm -rf "$BUILD"
fi

if [ "$CONFIGURE" = "yes" ] || [ ! -f "$BUILD/CMakeCache.txt" ]; then
  echo "Configuring ($CONFIG) -> $BUILD"
  cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE="$CONFIG"
fi

echo "Building $CONFIG ..."
for t in $TARGETS; do
  echo "  -> $t"
  cmake --build "$BUILD" --target "$t" -j "$(nproc)"
done

echo ""
echo "Build OK ($CONFIG) -> $BUILD"
'@

$buildScript = $buildScript `
    -replace '__REPO__', $WslRepo `
    -replace '__BUILD__', $WslBuild `
    -replace '__CONFIG__', $Configuration `
    -replace '__CONFIGURE__', $configureFlag `
    -replace '__CLEAN__', $cleanFlag `
    -replace '__TARGETS__', $targetCsv

Write-Host "WSL build ($Configuration) -> $BuildDir"
Invoke-WslBash $buildScript

if ($Test) {
    $testScript = @'
set -euo pipefail
cd "__REPO__"
ctest --test-dir "__BUILD__" --output-on-failure
'@
    $testScript = $testScript `
        -replace '__REPO__', $WslRepo `
        -replace '__BUILD__', $WslBuild

    Write-Host ""
    Write-Host "Running ctest ..."
    Invoke-WslBash $testScript
}
