# Build LeapDeviceFirmware (Release) via GNU Make after a Studio Release build.
param(
    [string]$Configuration = "Release",
    [string]$MotionStackRoot = ""
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\import_project_template.ps1" -MotionStackRoot $MotionStackRoot

$FirmwareDir = Join-Path $PSScriptRoot "LeapDeviceFirmware"
$ClearcoreRoot = $PSScriptRoot

$makePaths = @(
    "C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\arm\arm-gnu-toolchain\bin\make.exe",
    "C:\Program Files\Microchip\xc32\v4.45\bin\make.exe"
)

$make = $null
foreach ($p in $makePaths) {
    if (Test-Path $p) { $make = $p; break }
}
if ($null -eq $make) {
    $makeCmd = Get-Command make -ErrorAction SilentlyContinue
    if ($makeCmd) { $make = $makeCmd.Source }
}

if ($null -eq $make) {
    Write-Host "GNU make not found. Build in Microchip Studio instead."
    exit 1
}

foreach ($proj in @("libClearCore", "LwIP")) {
    Push-Location (Join-Path $ClearcoreRoot $proj)
    & $make $Configuration
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }
    Pop-Location
}

$makefile = Join-Path $FirmwareDir "$Configuration\Makefile"
if (-not (Test-Path $makefile)) {
    Write-Host "No $Configuration/Makefile yet. Build LeapDeviceFirmware once in Microchip Studio."
    exit 1
}

Push-Location (Join-Path $FirmwareDir $Configuration)
& $make all
$buildExit = $LASTEXITCODE
Pop-Location
if ($buildExit -ne 0) { exit $buildExit }

Write-Host "Built: $FirmwareDir\$Configuration\LeapDeviceFirmware.bin"
