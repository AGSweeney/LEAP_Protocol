#Requires -Version 5.1
<#
.SYNOPSIS
  Build LEAP Conformance Studio Release payload and compile the Inno Setup installer.

.DESCRIPTION
  1. Regenerates multi-size icons (fixes 16/32 px title-bar embedding)
  2. CMake build leap_studio_qt (Release) + windeployqt
  3. Compiles installer/leap_studio_setup.iss with ISCC.exe

.PARAMETER QtRoot
  Qt kit root, e.g. C:\Qt\6.8.3\msvc2022_64

.PARAMETER SkipBuild
  Only compile the installer from an existing Release folder.

.PARAMETER SkipIcons
  Skip icon regeneration.

.PARAMETER InnoSetupPath
  Path to ISCC.exe (auto-detected when omitted).

.EXAMPLE
  .\leap_studio_qt\installer\build_installer.ps1

.EXAMPLE
  .\leap_studio_qt\installer\build_installer.ps1 -SkipBuild
#>
[CmdletBinding()]
param(
    [string] $QtRoot = 'C:\Qt\6.8.3\msvc2022_64',
    [string] $BuildDir = 'D:\LEAP_Protocol\build-win',
    [string] $Config = 'Release',
    [string] $InnoSetupPath = '',
    [switch] $SkipBuild,
    [switch] $SkipIcons
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$studioRoot = Join-Path $repoRoot 'leap_studio_qt'
$stageDir = Join-Path $BuildDir 'leap_studio_qt\Release'
$distDir = Join-Path $repoRoot 'dist'
$issFile = Join-Path $PSScriptRoot 'leap_studio_setup.iss'
$iconScript = Join-Path $studioRoot 'resources\icons\generate_icons.py'
$appVersion = '1.0.0'

function Resolve-InnoSetupCompiler {
    param([string] $ExplicitPath)

    if ($ExplicitPath -and (Test-Path -LiteralPath $ExplicitPath)) {
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    throw 'Inno Setup compiler (ISCC.exe) not found. Install Inno Setup 6 or pass -InnoSetupPath.'
}

function Get-GitShortHash {
    try {
        Push-Location $repoRoot
        $hash = (& git rev-parse --short HEAD 2>$null)
        if ($LASTEXITCODE -eq 0 -and $hash) {
            return $hash.Trim()
        }
    }
    finally {
        Pop-Location
    }
    return 'unknown'
}

Write-Host ''
Write-Host ('=' * 72) -ForegroundColor DarkCyan
Write-Host 'LEAP Conformance Studio — installer build' -ForegroundColor Cyan
Write-Host ('=' * 72) -ForegroundColor DarkCyan
Write-Host "Publisher:  Adam G. Sweeney"
Write-Host "Version:    $appVersion"
Write-Host "Stage dir:  $stageDir"
Write-Host "Output dir: $distDir"
Write-Host "Git:        $(Get-GitShortHash)"
Write-Host "Time:       $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ''

if (-not $SkipIcons) {
    Write-Host 'Regenerating icon PNG/ICO assets...' -ForegroundColor Yellow
    & python $iconScript
    if ($LASTEXITCODE -ne 0) {
        throw "Icon generation failed (exit $LASTEXITCODE)"
    }
    $rcFile = Join-Path $studioRoot 'resources\leap_studio.rc'
    if (Test-Path -LiteralPath $rcFile) {
        (Get-Item -LiteralPath $rcFile).LastWriteTime = Get-Date
    }
}

if (-not $SkipBuild) {
    $needsConfigure = -not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))
    if (-not $needsConfigure) {
        $cacheText = Get-Content -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt') -Raw
        if ($cacheText -match 'Qt6_DIR:PATH=Qt6_DIR-NOTFOUND') {
            Write-Host 'CMake cache missing Qt6 — reconfiguring...' -ForegroundColor Yellow
            $needsConfigure = $true
        }
    }

    $buildScript = Join-Path $repoRoot 'build.ps1'
    if (-not (Test-Path -LiteralPath $buildScript)) {
        throw "build.ps1 not found at $buildScript"
    }

    $buildArgs = @(
        '-File', $buildScript,
        '-BuildDir', (Split-Path -Leaf $BuildDir),
        '-QtRoot', $QtRoot,
        '-Configuration', $Config,
        '-Target', 'leap_studio_qt'
    )
    if ($needsConfigure) {
        $buildArgs += '-Configure'
    }

    Write-Host "Building leap_studio_qt ($Config) via build.ps1..." -ForegroundColor Yellow
    & powershell -NoProfile -ExecutionPolicy Bypass @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed (exit $LASTEXITCODE)"
    }

    $exePath = Join-Path $stageDir 'leap_studio_qt.exe'
    if (-not (Test-Path -LiteralPath $exePath)) {
        throw "Expected executable missing: $exePath"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $stageDir 'leap_studio_qt.exe'))) {
    throw "Stage folder is missing leap_studio_qt.exe. Build first or omit -SkipBuild."
}

if (-not (Test-Path -LiteralPath (Join-Path $stageDir 'Qt6Core.dll'))) {
    throw "Stage folder is missing Qt6Core.dll. windeployqt did not run successfully."
}

if (-not (Test-Path -LiteralPath (Join-Path $studioRoot 'resources\icons\leap_studio.ico'))) {
    throw 'Installer icon missing: leap_studio_qt/resources/icons/leap_studio.ico'
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$iscc = Resolve-InnoSetupCompiler -ExplicitPath $InnoSetupPath
Write-Host "Compiling installer with: $iscc" -ForegroundColor Yellow

$stageDirFull = (Resolve-Path -LiteralPath $stageDir).Path
$distDirFull = (Resolve-Path -LiteralPath $distDir).Path

& $iscc `
    "/DStageDir=$stageDirFull" `
    "/DAppVersion=$appVersion" `
    "/DOutputDir=$distDirFull" `
    $issFile

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compile failed (exit $LASTEXITCODE)"
}

$setupExe = Get-ChildItem -LiteralPath $distDir -Filter 'LEAP_Conformance_Studio_*_x64_Setup.exe' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($null -eq $setupExe) {
    throw "Installer EXE not found in $distDir"
}

Write-Host ''
Write-Host "Installer ready:" -ForegroundColor Green
Write-Host "  $($setupExe.FullName)"
Write-Host "  $([math]::Round($setupExe.Length / 1MB, 2)) MB"
Write-Host ''
