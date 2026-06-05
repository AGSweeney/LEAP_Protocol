#Requires -Version 5.1
<#
.SYNOPSIS
  Deploy leap_studio_qt with windeployqt (Qt DLLs, plugins, MSVC runtime).

.PARAMETER QtRoot
  Qt kit root, e.g. C:\Qt\6.8.3\msvc2022_64

.PARAMETER ExePath
  Path to leap_studio_qt.exe (default: build-win Release output)
#>
[CmdletBinding()]
param(
    [string] $QtRoot = 'C:\Qt\6.8.3\msvc2022_64',
    [string] $ExePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExePath) {
    $ExePath = Join-Path $repoRoot 'build-win\leap_studio_qt\Release\leap_studio_qt.exe'
}

$windeploy = Join-Path $QtRoot 'bin\windeployqt.exe'
if (-not (Test-Path -LiteralPath $windeploy)) {
    throw "windeployqt not found: $windeploy"
}
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Executable not found: $ExePath (build leap_studio_qt first)"
}

Write-Host "windeployqt: $windeploy"
Write-Host "target:      $ExePath"

& $windeploy --release --compiler-runtime --no-translations $ExePath
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed (exit $LASTEXITCODE)"
}

Write-Host "Deploy complete: $(Split-Path -Parent $ExePath)"
