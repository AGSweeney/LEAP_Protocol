# Invoke LeapOS build pipeline from Windows via WSL.
$ErrorActionPreference = "Stop"

$LeapOsRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Resolve-Path (Join-Path $LeapOsRoot "..\..\..\..")
$BuildDir = Join-Path $LeapOsRoot "rtems-build"
$Mode = if ($args.Count -gt 0) { $args[0] } else { "all" }

$WslRepo = wsl wslpath -a $RepoRoot
$WslBuild = "$WslRepo/platforms/x86-32/D945GSEJT/LeapOS/rtems-build"

Write-Host "LeapOS build via WSL (mode=$Mode)"
Write-Host "  repo: $RepoRoot"

wsl bash -lc "cd '$WslBuild' && sed -i 's/\r$//' *.sh 2>/dev/null; bash build-all.sh $Mode"

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ImageDir = Join-Path $LeapOsRoot "rtems-image"
Write-Host ""
Write-Host "Artifacts:"
Get-ChildItem -Path $ImageDir -Include "leapos-device.iso","leapos-device.img","leap-port.exe","net-probe.exe" -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host "  $($_.FullName) ($([math]::Round($_.Length/1MB, 2)) MB)" }
