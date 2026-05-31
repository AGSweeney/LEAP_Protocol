# Wire ClearCore-Motion-Stack libraries next to LeapDeviceFirmware for Microchip Studio.
param(
    [string]$MotionStackRoot = "",
    [string]$VendorRoot = "$PSScriptRoot\vendor",
    [string]$RepoUrl = "https://github.com/AGSweeney/ClearCore-Motion-Stack.git"
)

$ErrorActionPreference = "Stop"

$ClearcoreRoot = $PSScriptRoot
$LeapRepoRoot = (Resolve-Path "$ClearcoreRoot\..\..").Path
$FirmwareDir = Join-Path $ClearcoreRoot "LeapDeviceFirmware"

function Get-FullPath {
    param([string]$Path)
    return [System.IO.Path]::GetFullPath($Path)
}

function Ensure-Junction {
    param(
        [string]$Link,
        [string]$Target
    )

    $targetFull = Get-FullPath $Target

    if (-not (Test-Path -LiteralPath $Target)) {
        throw "Junction target missing: $Target"
    }

    if (Test-Path -LiteralPath $Link) {
        $item = Get-Item -LiteralPath $Link -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            $existing = $item.Target
            if ($existing -is [array]) { $existing = $existing[0] }
            if ($existing -ieq $targetFull) {
                return
            }
            Remove-Item -LiteralPath $Link -Force
        }
        else {
            throw "Path exists and is not a junction: $Link"
        }
    }

    New-Item -ItemType Junction -Path $Link -Target $targetFull | Out-Null
}

function Resolve-ProjectTemplate {
    param([string]$RootHint)

    $candidates = @()

    if ($RootHint -ne "") {
        $candidates += (Join-Path $RootHint "ProjectTemplate")
        $candidates += $RootHint
    }

    $candidates += "D:\ClearCore-Motion-Stack\ProjectTemplate"
    $candidates += (Join-Path $VendorRoot "ClearCore-Motion-Stack\ProjectTemplate")

    foreach ($candidate in $candidates) {
        $full = Get-FullPath $candidate
        if ((Test-Path (Join-Path $full "libClearCore\ClearCore.cppproj")) -and
            (Test-Path (Join-Path $full "LwIP\LwIP.cppproj")))
        {
            return $full
        }
    }

    return $null
}

function Install-LwipLeapHook {
    param([string]$ProjectTemplateRoot)

    $src = Join-Path $FirmwareDir "lwip_hooks.h"
    $dst = Join-Path $ProjectTemplateRoot "LwIP\LwIP\port\include\lwip_hooks.h"
    if (-not (Test-Path $src)) {
        throw "Missing LEAP lwIP hook header: $src"
    }
    Copy-Item -Path $src -Destination $dst -Force
}

if (-not (Test-Path $FirmwareDir)) {
    throw "Missing firmware project: $FirmwareDir"
}

$ProjectTemplate = Resolve-ProjectTemplate -RootHint $MotionStackRoot
if ($null -eq $ProjectTemplate) {
    New-Item -ItemType Directory -Path $VendorRoot -Force | Out-Null
    $vendorRepo = Join-Path $VendorRoot "ClearCore-Motion-Stack"
    if (-not (Test-Path $vendorRepo)) {
        Write-Host "Cloning $RepoUrl ..."
        git clone --depth 1 $RepoUrl $vendorRepo
    }
    $ProjectTemplate = Resolve-ProjectTemplate -RootHint $vendorRepo
}

if ($null -eq $ProjectTemplate) {
    throw "Could not locate ProjectTemplate (libClearCore + LwIP). Set -MotionStackRoot to your ClearCore-Motion-Stack checkout."
}

Write-Host "Using ProjectTemplate: $ProjectTemplate"

# Siblings of LeapDeviceFirmware/ — paths used by LeapDeviceFirmware.atsln (..\libClearCore, etc.)
Ensure-Junction -Link (Join-Path $ClearcoreRoot "libClearCore") `
    -Target (Join-Path $ProjectTemplate "libClearCore")
Ensure-Junction -Link (Join-Path $ClearcoreRoot "LwIP") `
    -Target (Join-Path $ProjectTemplate "LwIP")
Ensure-Junction -Link (Join-Path $ClearcoreRoot "ClearLinkCompatibilityFirmware") `
    -Target (Join-Path $ProjectTemplate "ClearLinkCompatibilityFirmware")
Ensure-Junction -Link (Join-Path $ClearcoreRoot "Tools") `
    -Target (Join-Path $ProjectTemplate "Tools")

Ensure-Junction -Link (Join-Path $FirmwareDir "leap_protocol") -Target $LeapRepoRoot

Install-LwipLeapHook -ProjectTemplateRoot $ProjectTemplate

$solution = Join-Path $FirmwareDir "LeapDeviceFirmware.atsln"

Write-Host ""
Write-Host "Libraries linked under platforms/clearcore/:"
Write-Host "  libClearCore, LwIP, ClearLinkCompatibilityFirmware, Tools"
Write-Host ""
Write-Host "Open in Microchip Studio:"
Write-Host "  $solution"
Write-Host ""
Write-Host "Build order (Release): ClearCore -> LwIP -> LeapDeviceFirmware"
Write-Host ""
