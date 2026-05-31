# wire_smoke_win.ps1 — Windows Npcap loopback smoke test.
#
# Usage (from repo root):
#   powershell -ExecutionPolicy Bypass -File tools/ci/wire_smoke_win.ps1
#
# Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
# SPDX-License-Identifier: MIT

$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Build = if ($env:LEAP_BUILD_DIR) { $env:LEAP_BUILD_DIR } else { Join-Path $Root "build-win" }
$Exe = Join-Path $Build "Release/leap_win_smoke.exe"
if (-not (Test-Path $Exe)) {
    $Exe = Join-Path $Build "Debug/leap_win_smoke.exe"
}
if (-not (Test-Path $Exe)) {
    $Exe = Join-Path $Build "leap_win_smoke.exe"
}

if ($env:LEAP_SKIP_WIRE_SMOKE -eq "1") {
    Write-Host "wire smoke: skipped — LEAP_SKIP_WIRE_SMOKE=1"
    exit 0
}

$Wpcap = Join-Path $env:SystemRoot "System32\Npcap\wpcap.dll"
if (-not (Test-Path $Wpcap)) {
    Write-Host "wire smoke: skipped — Npcap wpcap.dll not found ($Wpcap)"
    exit 0
}

$Cmake = $env:LEAP_CMAKE
if (-not $Cmake) {
    $CmakeGuess = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $CmakeGuess) {
        $Cmake = $CmakeGuess
    } else {
        $Cmake = "cmake"
    }
}

if (-not (Test-Path $Exe)) {
    Write-Host "wire smoke: building..."
    & $Cmake -S $Root -B $Build -DLEAP_BUILD_WIN_SMOKE=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $Cmake --build $Build --config Release --target leap_win_smoke
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $Exe = Join-Path $Build "Release/leap_win_smoke.exe"
}

if (-not (Test-Path $Exe)) {
    Write-Error "wire smoke: missing $Exe"
}

Write-Host "wire smoke: running $Exe"
& $Exe @args
exit $LASTEXITCODE
