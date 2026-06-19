# Build leap_core + shared LeapGateway sources as a static ARM library for
# NetBurner MODM7AE70 LEAP Gateway (embedded LeapOS-Gateway).
#
# Usage: .\build-leap-gateway-netburner.ps1

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir '..\..\..\..')).Path
$LeapCore = Join-Path $RepoRoot 'leap_core'
$SharedGw = Join-Path $RepoRoot 'platforms\x86-32\D945GSEJT\LeapGateway\src'
$PlatformDir = Join-Path $ScriptDir 'src\platform'
$GenDir = Join-Path $PlatformDir 'generated'
$BuildDir = if ($env:LEAP_GW_BUILD_DIR) { $env:LEAP_GW_BUILD_DIR } else { Join-Path $env:TEMP 'leap-gateway-netburner' }
$ObjDir = Join-Path $BuildDir 'objs'
$OutLib = Join-Path $ScriptDir 'leap\build-work\libleap_gateway_netburner.a'
$NndkRoot = if ($env:NNDK_ROOT) { $env:NNDK_ROOT } else { 'C:\nburn' }
$ToolchainBin = Join-Path $NndkRoot 'gcc\bin'
$Gcc = Join-Path $ToolchainBin 'arm-unknown-eabi-gcc.exe'
$Gxx = Join-Path $ToolchainBin 'arm-unknown-eabi-g++.exe'
$Ar = Join-Path $ToolchainBin 'arm-unknown-eabi-ar.exe'
$Ranlib = Join-Path $ToolchainBin 'arm-unknown-eabi-ranlib.exe'

if (-not (Test-Path $Gcc)) { throw "arm toolchain not found at $Gcc" }
if (-not (Test-Path $LeapCore)) { throw "leap_core not found at $LeapCore" }
if (-not (Test-Path $SharedGw)) { throw "LeapGateway shared sources not found at $SharedGw" }

$NbInc = @(
    '-IC:/nburn/nbrtos/include',
    '-IC:/nburn/platform/MODM7AE70/include',
    '-IC:/nburn/arch/cortex-m7/include',
    '-IC:/nburn/arch/cortex-m7/cpu/SAME70/include',
    '-IC:/nburn/libraries/include'
) -join ' '

$GwInc = @(
    "-I$PlatformDir",
    "-I$GenDir",
    "-I$SharedGw",
    "-I$(Join-Path $LeapCore 'inc')"
) -join ' '

$Defines = '-DNETBURNER_GATEWAY=1 -DLEAP_GATEWAY_OPENER_ENABLE=1 -DMODM7AE70 -DSAME70 -DCORTEX_M7'
$ArchFlags = '-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb'
$Cflags = "$ArchFlags -O2 -Wall -std=gnu17 $Defines $NbInc $GwInc"
$Cxxflags = "$ArchFlags -O2 -Wall -std=gnu++17 -fno-exceptions -fno-rtti $Defines $NbInc $GwInc"

$CoreSrc = @(
    'src/crc/leap_crc.c',
    'src/frame/leap_frame.c',
    'src/services/disc/leap_disc_controller.c',
    'src/services/dir/leap_dir_controller.c',
    'src/services/dir/leap_dir_controller_capabilities.c',
    'src/services/mgmt/leap_mgmt_controller.c',
    'src/services/mgmt/leap_mgmt_process.c',
    'src/services/mgmt/leap_mgmt_device.c',
    'src/services/pd/leap_pd_controller.c',
    'src/services/pd/leap_pd_common.c',
    'src/services/diag/leap_diag_controller.c',
    'src/leap_controller_stack.c',
    'src/leap_controller_session_hub.c',
    'src/leap_controller_peer.c',
    'src/leap_controller_sequence.c',
    'src/bridge/leap_eip_bridge.c',
    'src/bridge/leap_gateway_config.c',
    'src/leap_log.c',
    'src/leap_build_info.c'
) | ForEach-Object { Join-Path $LeapCore $_ }

$SharedSrc = @(
    'gateway_global.c',
    'gateway_leap_session.c',
    'gateway_rtems_io.c',
    'gateway_pd_io.c',
    'leap_gateway_opener.c'
) | ForEach-Object { Join-Path $SharedGw $_ }

$PlatformCpp = @(
    'leap_time_nb.cpp',
    'gateway_net_nb.cpp',
    'gateway_storage_nb.cpp',
    'gateway_leap_session_nb.cpp'
) | ForEach-Object { Join-Path $PlatformDir $_ }

Write-Host '=== Building libleap_gateway_netburner.a ==='
if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path $OutLib) -Force | Out-Null

$Objs = @()
foreach ($src in ($CoreSrc + $SharedSrc)) {
    $base = [IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $ObjDir "$base.o"
    Write-Host "  CC $base"
    & $Gcc $Cflags.Split(' ') -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $src" }
    $Objs += $obj
}

foreach ($src in $PlatformCpp) {
    $base = [IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $ObjDir "$base.o"
    Write-Host "  CXX $base"
    & $Gxx $Cxxflags.Split(' ') -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $src" }
    $Objs += $obj
}

if (Test-Path $OutLib) { Remove-Item -Force $OutLib }
& $Ar rcs $OutLib @Objs
& $Ranlib $OutLib
Write-Host "Installed $OutLib ($($Objs.Count) objects)"
