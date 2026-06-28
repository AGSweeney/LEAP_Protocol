# Verify OpENer CMake builds (host + NetBurner cross-compile configure)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Source = Join-Path $Root "source"

Write-Host "=== Stub core build ==="
$CoreBuild = Join-Path $Root "build-verify-core"
cmake -S $Source -B $CoreBuild `
  -DOPENER_BUILD_NETWORK_LAYER=OFF `
  -DOPENER_NET_BACKEND=stub
cmake --build $CoreBuild --config Release
Write-Host "OK: stub core build"

Write-Host "=== CMake validation (expect failures) ==="
$FailStub = Join-Path $Root "build-verify-fail-stub"
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cmake -S $Source -B $FailStub -DOPENER_NET_BACKEND=stub -DOPENER_BUILD_NETWORK_LAYER=ON 2>&1 | Out-Null
$failCode = $LASTEXITCODE
$ErrorActionPreference = $prevEap
if ($failCode -eq 0) { throw "stub+network layer should fail configure" }
Write-Host "OK: stub + network layer rejected"

if (Test-Path "C:\nburn\nbrtos\include") {
  Write-Host "=== NetBurner cross-compile configure ==="
  $env:PATH = "C:\nburn\gcc\bin;$env:PATH"
  $NbBuild = Join-Path $Root "build-verify-nb"
  cmake -S $Source -B $NbBuild `
    -DCMAKE_TOOLCHAIN_FILE="$Source/buildsupport/Toolchain/Toolchain-NetBurner-NNDK.cmake" `
    -DOPENER_NNDK_ROOT=C:/nburn `
    -DOPENER_NET_BACKEND=netburner `
    -DOPENER_BUILD_NETWORK_LAYER=ON `
    -DOPENER_LLDP=ON `
    -DOPENER_NB_LLDP=ON `
    -DOPENER_NB_ACD=ON `
    -G "Unix Makefiles"
  if ($LASTEXITCODE -ne 0) { throw "NetBurner cross-compile configure failed" }
  Write-Host "OK: NetBurner toolchain configure"
  cmake --build $NbBuild -j 4
  if ($LASTEXITCODE -ne 0) { throw "NetBurner cross-compile build failed" }
  $LibDir = Join-Path $NbBuild "lib"
  if (-not (Test-Path $LibDir)) { throw "Expected lib output directory: $LibDir" }
  Write-Host "OK: libraries in $LibDir"
} else {
  Write-Host "SKIP: C:/nburn not found - NetBurner cross-compile not tested"
}

Write-Host "All verification steps passed."
