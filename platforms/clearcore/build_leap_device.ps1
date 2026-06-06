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

# Studio-generated Makefiles may still reference the legacy leap_protocol/inc layout.
# leap_protocol is a junction to the LEAP repo root; headers and sources live under leap_core/.
$content = Get-Content $makefile -Raw
$patched = $content `
    -replace 'leap_protocol\\inc', 'leap_protocol\leap_core\inc' `
    -replace 'leap_protocol/src', 'leap_protocol/leap_core/src'
if ($patched -ne $content) {
    Set-Content -Path $makefile -Value $patched -NoNewline
    Write-Host "Patched $Configuration/Makefile leap_core include and source paths."
}

$buildDir = Join-Path $FirmwareDir $Configuration
foreach ($subdir in @(
        "leap_protocol\leap_core\src\crc",
        "leap_protocol\leap_core\src\frame",
        "leap_protocol\leap_core\src\services\diag",
        "leap_protocol\leap_core\src\services\dir",
        "leap_protocol\leap_core\src\services\disc",
        "leap_protocol\leap_core\src\services\mgmt",
        "leap_protocol\leap_core\src\services\pd",
        "leap_protocol\leap_core\src"
    )) {
    New-Item -ItemType Directory -Force -Path (Join-Path $buildDir $subdir) | Out-Null
}

Push-Location $buildDir
& $make all
$buildExit = $LASTEXITCODE
Pop-Location
if ($buildExit -ne 0) { exit $buildExit }

Write-Host "Built: $FirmwareDir\$Configuration\LeapDeviceFirmware.bin"
