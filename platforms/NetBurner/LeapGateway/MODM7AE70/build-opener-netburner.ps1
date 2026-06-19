# Build OpENer as a static ARM library for LEAP Gateway on NetBurner MODM7AE70.
# Stages OpENer-Enhanced, injects the NETBURNER port overlay, and merges libs
# into opener/build-work/libopener_netburner.a
#
# Usage: .\build-opener-netburner.ps1
# Env:   OPENER_ROOT, NNDK_ROOT, LEAP_OPENER_BUILD_DIR, CMAKE_EXE (optional)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OpenerRoot = if ($env:OPENER_ROOT) { $env:OPENER_ROOT } else { 'D:\OpENer-Enhanced' }
$NndkRoot = if ($env:NNDK_ROOT) { $env:NNDK_ROOT } else { 'C:\nburn' }
$BuildDir = if ($env:LEAP_OPENER_BUILD_DIR) { $env:LEAP_OPENER_BUILD_DIR } else { Join-Path $env:TEMP 'leap-opener-netburner' }
$StageDir = Join-Path $BuildDir 'opener-src'
$GateH = Join-Path $ScriptDir 'opener\opener_netburner_gate.h'
$OutLib = Join-Path $ScriptDir 'opener\build-work\libopener_netburner.a'
$ToolchainBin = Join-Path $NndkRoot 'gcc\bin'
$Gcc = Join-Path $ToolchainBin 'arm-unknown-eabi-gcc.exe'
$Gxx = Join-Path $ToolchainBin 'arm-unknown-eabi-g++.exe'
$Ar = Join-Path $ToolchainBin 'arm-unknown-eabi-ar.exe'
$Ranlib = Join-Path $ToolchainBin 'arm-unknown-eabi-ranlib.exe'

if (-not (Test-Path (Join-Path $OpenerRoot 'source'))) {
    throw "OpENer source not found at $OpenerRoot. Set OPENER_ROOT to your OpENer-Enhanced checkout."
}

if (-not (Test-Path $Gcc)) {
    throw "arm toolchain not found at $Gcc"
}

$Cmake = if ($env:CMAKE_EXE) { $env:CMAKE_EXE } else { Join-Path $ToolchainBin 'cmake.exe' }
if (-not (Test-Path $Cmake)) {
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakeCmd) {
        throw 'cmake not found. Set CMAKE_EXE to full cmake.exe path or add cmake to PATH.'
    }
    $Cmake = $cmakeCmd.Source
}

$NbCflags = "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb -O2 -I$StageDir -include opener_netburner_gate.h -DCIP_FILE_OBJECT=0 -DCIP_SECURITY_OBJECTS=0 -DOPENER_NETBURNER -DMODM7AE70 -DSAME70 -DCORTEX_M7"
$NbCxxflags = "$NbCflags -fno-exceptions -fno-rtti -std=gnu++17"

Write-Host '=== Staging OpENer source ==='
if (Test-Path $StageDir) {
    Remove-Item -Recurse -Force $StageDir
}
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null
robocopy (Join-Path $OpenerRoot 'source') $StageDir /E /XD build-work 'build-*' /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "robocopy failed staging OpENer source (exit $LASTEXITCODE)"
}

Write-Host '=== Injecting NETBURNER platform port ==='
robocopy (Join-Path $ScriptDir 'opener\netburner_port') (Join-Path $StageDir 'src\ports\NETBURNER') /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "robocopy failed injecting NETBURNER port (exit $LASTEXITCODE)"
}
$nbSupport = Join-Path $StageDir 'buildsupport\NETBURNER'
New-Item -ItemType Directory -Path $nbSupport -Force | Out-Null
Copy-Item (Join-Path $ScriptDir 'opener\OpENer_PLATFORM_INCLUDES.cmake') $nbSupport -Force
Copy-Item $GateH (Join-Path $StageDir 'opener_netburner_gate.h') -Force

Write-Host '=== Copying Connection Manager stats module ==='
Copy-Item (Join-Path $ScriptDir 'opener\netburner_port\cipconnectionmanager_stats.c') (Join-Path $StageDir 'src\cip\') -Force
Copy-Item (Join-Path $ScriptDir 'opener\netburner_port\cipconnectionmanager_stats.h') (Join-Path $StageDir 'src\cip\') -Force

Write-Host '=== Copying NetBurner NV data modules ==='
Copy-Item (Join-Path $ScriptDir 'opener\netburner_port\nvtcpip.c') (Join-Path $StageDir 'src\ports\nvdata\') -Force
Copy-Item (Join-Path $ScriptDir 'opener\netburner_port\nvdata.c') (Join-Path $StageDir 'src\ports\nvdata\') -Force
Copy-Item (Join-Path $ScriptDir 'opener\netburner_port\nb_nvtcpip.h') (Join-Path $StageDir 'src\ports\nvdata\') -Force

Write-Host '=== Patching staged OpENer for NETBURNER ==='
& (Join-Path $ScriptDir 'opener\patch-opener-netburner.ps1') -StageDir $StageDir

$CmakeDir = Join-Path $BuildDir 'cmake'
Write-Host '=== Configuring OpENer (NETBURNER, static ARM) ==='
$buildsupport = (Join-Path $StageDir 'buildsupport') -replace '\\', '/'
& $Cmake -S $StageDir -B $CmakeDir -G 'Unix Makefiles' `
    -DOpENer_PLATFORM=NETBURNER `
    -DOpENer_TESTS=OFF `
    -DOpENer_TRACES=OFF `
    -DOPENER_INSTALL_AS_LIB=OFF `
    -DOpENer_BUILDSUPPORT_DIR="$buildsupport" `
    -DOpENer_Device_Config_Device_Name='LEAP-Gateway' `
    -DOpENer_CIP_OBJECT_CIP_FILE_OBJECT=OFF `
    -DCIP_FILE_OBJECT=OFF `
    -DCIP_SECURITY_OBJECTS=OFF `
    -DOPENER_IS_DLR_DEVICE=OFF `
    -DOPENER_LLDP=OFF `
    -DCMAKE_C_COMPILER="$Gcc" `
    -DCMAKE_CXX_COMPILER="$Gxx" `
    -DCMAKE_AR="$Ar" `
    -DCMAKE_RANLIB="$Ranlib" `
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY `
    -DCMAKE_C_FLAGS="$NbCflags" `
    -DCMAKE_CXX_FLAGS="$NbCxxflags"
if ($LASTEXITCODE -ne 0) {
    throw 'cmake configure failed'
}

Write-Host '=== Building OpENer ==='
& $Cmake --build $CmakeDir -- -j12
if ($LASTEXITCODE -ne 0) {
    throw 'cmake build failed'
}

$MergeDir = Join-Path $BuildDir 'merge'
if (Test-Path $MergeDir) {
    Remove-Item -Recurse -Force $MergeDir
}
New-Item -ItemType Directory -Path $MergeDir -Force | Out-Null

$OpenerLibs = @(
    'src/cip/libCIP.a'
    'src/enet_encap/libENET_ENCAP.a'
    'src/utils/libUtils.a'
    'src/ports/nvdata/libNVDATA.a'
    'src/ports/libPLATFORM_GENERIC.a'
    'src/ports/NETBURNER/libNETBURNERPLATFORM.a'
    'src/ports/NETBURNER/leap_gateway/libLeapGateway.a'
)

foreach ($lib in $OpenerLibs) {
    $libPath = Join-Path $CmakeDir $lib
    if (-not (Test-Path $libPath)) {
        throw "missing OpENer library: $libPath"
    }
    Push-Location $MergeDir
    try {
        & $Ar x $libPath
        if ($LASTEXITCODE -ne 0) {
            throw "ar extract failed for $libPath"
        }
    } finally {
        Pop-Location
    }
}

$objs = Get-ChildItem -Path $MergeDir -Filter '*.obj'
if ($objs.Count -eq 0) {
    throw 'no OpENer object files extracted for merge'
}

$outDir = Split-Path -Parent $OutLib
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
if (Test-Path $OutLib) {
    Remove-Item -Force $OutLib
}

& $Ar crs $OutLib ($objs | ForEach-Object { $_.FullName })
if ($LASTEXITCODE -ne 0) {
    throw 'ar merge failed'
}

& $Ranlib $OutLib
if ($LASTEXITCODE -ne 0) {
    throw 'ranlib failed'
}

Write-Host ''
Write-Host "Installed $OutLib ($($objs.Count) objects)"
