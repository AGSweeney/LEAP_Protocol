#Requires -Version 5.1
<#
.SYNOPSIS
  Automated LEAP bench tests for GL-C-618WL (ESP32) over Windows Npcap.

.DESCRIPTION
  Thin wrapper around leap_conformance.exe (shared C conformance engine).
  USB bench: unplug LED strip before running (PD output fills). Use -Force to skip pause.

  -Report writes markdown via leap_conformance --report-md.
#>
[CmdletBinding()]
param(
    [string] $Adapter = '',
    [string] $NicName = 'Ethernet 3',
    [string] $ExpectedMac = '94:51:dc:21:f0:2f',
    [string] $ToolsDir = 'D:\LEAP_Protocol\build-win\leap_cli\Release',
    [int]    $CyclicSeconds = 2,
    [int]    $BootstrapRetries = 3,
    [int]    $RetryDelayMs = 1000,
    [switch] $Force,
    [switch] $Report,
    [string] $ReportPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ConformanceExe = Join-Path $ToolsDir 'leap_conformance.exe'

function Resolve-NpcapAdapter {
    param(
        [string] $AdapterPath,
        [string] $NetAdapterName
    )

    if ($AdapterPath -and $AdapterPath -notmatch '^\s*$') {
        return $AdapterPath
    }

    $nic = Get-NetAdapter -Name $NetAdapterName -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $nic) {
        throw "Net adapter '$NetAdapterName' not found. Run Get-NetAdapter or leap_conformance.exe --list-adapters"
    }

    $guid = $nic.InterfaceGuid.ToString().ToUpperInvariant()
    return '\Device\NPF_{0}' -f $guid
}

Write-Host ''
Write-Host ('=' * 72) -ForegroundColor DarkCyan
Write-Host 'LEAP GL-C-618WL automated bench test (leap_conformance)' -ForegroundColor Cyan
Write-Host ('=' * 72) -ForegroundColor DarkCyan

try {
    $Adapter = Resolve-NpcapAdapter -AdapterPath $Adapter -NetAdapterName $NicName
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host "Adapter:      $Adapter (from $NicName)"
Write-Host "Expected MAC: $ExpectedMac"
Write-Host "Tools:        $ToolsDir"
Write-Host "Time:         $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

if (-not (Test-Path -LiteralPath $ConformanceExe)) {
    Write-Host "leap_conformance.exe not found at $ConformanceExe" -ForegroundColor Red
    Write-Host 'Build: cmake -B build-win -DLEAP_BUILD_WIN_L2=ON; cmake --build build-win --config Release' -ForegroundColor Yellow
    exit 1
}

Write-Host ''
Write-Host 'POWER: USB-only bench runs must have the LED strip UNPLUGGED.' -ForegroundColor Yellow
Write-Host '       Tests send PD output fills; a connected strip can reboot the ESP32.' -ForegroundColor Yellow
if (-not $Force) {
    Write-Host '       Ctrl+C to abort, or waiting 5s (use -Force to skip)...' -ForegroundColor DarkYellow
    Start-Sleep -Seconds 5
}
else {
    Write-Host '       -Force: skipping startup pause.' -ForegroundColor DarkGray
}

$reportFile = $ReportPath
if ($Report -and -not $reportFile) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $reportFile = Join-Path $PSScriptRoot "reports\leap_bench_$stamp.md"
}

$args = @(
    '--adapter', $Adapter,
    '--scenario', 'glc618wl_bench_v1',
    '--peer-mac', $ExpectedMac,
    '--cyclic-seconds', "$CyclicSeconds",
    '--bootstrap-retries', "$BootstrapRetries",
    '--retry-delay-ms', "$RetryDelayMs"
)

if ($reportFile) {
    $args += @('--report-md', $reportFile)
}

& $ConformanceExe @args
exit $LASTEXITCODE
