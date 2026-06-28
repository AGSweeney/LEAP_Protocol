# Cross-compile OpENer static libraries for NetBurner firmware
param(
  [ValidateSet("Core", "Extended")]
  [string]$Profile = "Core",
  [string]$BuildDir = "",
  [string]$NndkRoot = "C:/nburn",
  [string]$Platform = "MODM7AE70",
  [string]$Cpu = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Source = Join-Path $Root "source"

if (-not $BuildDir) {
  $BuildDir = "build-nb-$Platform"
}
$Out = Join-Path $Root $BuildDir

$env:PATH = "C:\nburn\gcc\bin;$env:PATH"

$coldfirePlatforms = @("MOD5441X", "NANO54415", "SB800EX")
$isColdFire = $coldfirePlatforms -contains $Platform

if (-not $Cpu) {
  if ($Platform -eq "SOMRT1061") { $Cpu = "MIMXRT10xx" }
  elseif ($Platform -eq "MODRT1171") { $Cpu = "MIMXRT11xx" }
  elseif ($isColdFire) { $Cpu = "MCF5441X" }
  else { $Cpu = "SAME70" }
}

if ($isColdFire) {
  $toolchainFile = "$Source/buildsupport/Toolchain/Toolchain-NetBurner-ColdFire.cmake"
  $arch = "coldfire"
} else {
  $toolchainFile = "$Source/buildsupport/Toolchain/Toolchain-NetBurner-NNDK.cmake"
  $arch = "cortex-m7"
}

$cmakeArgs = @(
  "-S", $Source,
  "-B", $Out,
  "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
  "-G", "Unix Makefiles",
  "-DOPENER_NNDK_ROOT=$NndkRoot",
  "-DOPENER_NNDK_PLATFORM=$Platform",
  "-DOPENER_NNDK_ARCH=$arch",
  "-DOPENER_NNDK_CPU=$Cpu",
  "-DOPENER_NET_BACKEND=netburner",
  "-DOPENER_BUILD_NETWORK_LAYER=ON",
  "-DOPENER_BUILD_PROFILE=$($Profile.ToLower())"
)

if ($Platform -in @("SOMRT1061", "MODRT1171", "MODM7AE70", "SBE70LC")) {
  $cmakeArgs += "-DOPENER_FLOAT_ABI=softfp"
}

if ($Profile -eq "Extended") {
  $cmakeArgs += @(
    "-DOPENER_LLDP=ON",
    "-DOPENER_NB_LLDP=ON",
    "-DOPENER_NB_ACD=ON"
  )
}

Write-Host "=== Configure ($Profile, $Platform) ==="
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "=== Build ==="
cmake --build $Out -j 4
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

$LibDir = Join-Path $Out "lib"
if (-not (Test-Path $LibDir)) { throw "Missing output: $LibDir" }

Write-Host "OK: libraries in $LibDir"
Get-ChildItem $LibDir -Filter "*.a" | ForEach-Object { Write-Host "  $($_.Name)" }
