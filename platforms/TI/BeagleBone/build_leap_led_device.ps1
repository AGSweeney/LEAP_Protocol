# Build BBB baremetal LEAP Device that maps PD digital outputs to USR LEDs.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File platforms/TI/BeagleBone/build_leap_led_device.ps1
#   powershell -ExecutionPolicy Bypass -File platforms/TI/BeagleBone/build_leap_led_device.ps1 -SdCardDrive H

param(
    [string]$StarterWareRoot = "C:\ti\AM335X_StarterWare_02_00_01_01",
    [string]$GccRoot = "C:\ti\gcc-arm-none-eabi-7-2018-q2-update",
    [string]$SdCardDrive = ""
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if (-not (Test-Path $Path)) {
        throw "$Label not found: $Path"
    }
    return (Resolve-Path $Path).Path
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$StarterWareRoot = Resolve-ExistingPath $StarterWareRoot "StarterWare"
$GccRoot = Resolve-ExistingPath $GccRoot "GCC toolchain"

$Gcc = Join-Path $GccRoot "bin\arm-none-eabi-gcc.exe"
$Objcopy = Join-Path $GccRoot "bin\arm-none-eabi-objcopy.exe"

foreach ($tool in @(
        @{ Path = $Gcc; Label = "arm-none-eabi-gcc" },
        @{ Path = $Objcopy; Label = "arm-none-eabi-objcopy" }
    )) {
    if (-not (Test-Path $tool.Path)) {
        throw "$($tool.Label) not found: $($tool.Path)"
    }
}

$LibRoot = Join-Path $StarterWareRoot "binary\armv7a\gcc\am335x"
$RequiredLibs = @(
    (Join-Path $LibRoot "drivers\Release\libdrivers.a"),
    (Join-Path $LibRoot "beaglebone\platform\Release\libplatform.a"),
    (Join-Path $LibRoot "system_config\Release\libsystem_config.a")
)

foreach ($lib in $RequiredLibs) {
    if (-not (Test-Path $lib)) {
        throw "StarterWare library missing: $lib - build a StarterWare example first"
    }
}

$SourceDir = Join-Path $PSScriptRoot "leap_led_device"
$OutDir = Join-Path $PSScriptRoot "out\leap_led_device"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$IncludeFlags = @(
    "-I$SourceDir",
    "-I$(Join-Path $RepoRoot 'inc')",
    "-I$(Join-Path $StarterWareRoot 'include')",
    "-I$(Join-Path $StarterWareRoot 'include\hw')",
    "-I$(Join-Path $StarterWareRoot 'include\armv7a\am335x')"
)

$CommonFlags = @(
    "-mcpu=cortex-a8",
    "-marm",
    "-Dam335x",
    "-Dbeaglebone",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-stack-protector",
    "-Wall",
    "-Wextra",
    "-Os"
) + $IncludeFlags

$Sources = @(
    (Join-Path $PSScriptRoot "boot\start.S"),
    (Join-Path $SourceDir "runtime.c"),
    (Join-Path $SourceDir "bbb_hw.c"),
    (Join-Path $SourceDir "bbb_timer.c"),
    (Join-Path $SourceDir "bbb_exc.c"),
    (Join-Path $SourceDir "bbb_cpsw_raw.c"),
    (Join-Path $SourceDir "main.c"),
    (Join-Path $RepoRoot "src\crc\leap_crc.c"),
    (Join-Path $RepoRoot "src\frame\leap_frame.c"),
    (Join-Path $RepoRoot "src\services\disc\leap_disc_device.c"),
    (Join-Path $RepoRoot "src\services\dir\leap_dir_device.c"),
    (Join-Path $RepoRoot "src\services\mgmt\leap_mgmt_device.c"),
    (Join-Path $RepoRoot "src\services\mgmt\leap_mgmt_process.c"),
    (Join-Path $RepoRoot "src\services\pd\leap_pd_device.c"),
    (Join-Path $RepoRoot "src\services\pd\leap_pd_common.c"),
    (Join-Path $RepoRoot "src\services\diag\leap_diag_device.c"),
    (Join-Path $RepoRoot "src\leap_device_stack.c")
)

$Objects = @()

Write-Host "Building BBB LEAP LED Device"
Write-Host "  StarterWare : $StarterWareRoot"
Write-Host "  GCC         : $GccRoot"
Write-Host "  Out         : $OutDir"
Write-Host ""

foreach ($src in $Sources) {
    $name = (Split-Path $src -Leaf)
    $obj = Join-Path $OutDir ($name -replace '\.(c|S)$', '.o')
    & $Gcc @CommonFlags -c $src -o $obj
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed: $src"
    }
    $Objects += $obj
}

$Elf = Join-Path $OutDir "leap_bbb_led_device.elf"
$Bin = Join-Path $OutDir "leap_bbb_led_device.bin"
$Map = Join-Path $OutDir "leap_bbb_led_device.map"
$Linker = Join-Path $PSScriptRoot "boot\linker.ld"

& $Gcc `
    -mcpu=cortex-a8 `
    -marm `
    -nostdlib `
    "-Wl,-T,$Linker" `
    "-Wl,-Map,$Map" `
    $Objects `
    (Join-Path $LibRoot "drivers\Release\libdrivers.a") `
    (Join-Path $LibRoot "beaglebone\platform\Release\libplatform.a") `
    (Join-Path $LibRoot "system_config\Release\libsystem_config.a") `
    "-L$(Join-Path $GccRoot 'lib\gcc\arm-none-eabi\7.3.1')" `
    -lgcc `
    -o $Elf
if ($LASTEXITCODE -ne 0) {
    throw "Link failed: $Elf"
}

& $Objcopy -O binary $Elf $Bin
if ($LASTEXITCODE -ne 0) {
    throw "Objcopy failed: $Bin"
}

$UbootTxt = @"
# BBB LEAP LED Device
#
# Raw LEAP EtherType 0x88B6 over CPSW. PD digital output bits map to:
# bit0=USR0, bit1=USR1, bit2=USR2, bit3=USR3.
#
fatload mmc 0:1 0x80000000 leap_bbb_led_device.bin
go 0x80000000
#
# From the master:
#   leap_win_controller.exe --outputs 0x0001 <adapter>
#   leap_win_controller.exe --outputs 0x0008 <adapter>
"@
Set-Content -Path (Join-Path $OutDir "uboot_leap_led_device.txt") -Value $UbootTxt -Encoding ASCII

if ($SdCardDrive -ne "") {
    $driveRoot = $SdCardDrive.TrimEnd('\', ':') + ':\'
    if (-not (Test-Path $driveRoot)) {
        throw "SD card drive not found: $driveRoot"
    }
    Copy-Item -Force $Bin (Join-Path $driveRoot "leap_bbb_led_device.bin")
    Copy-Item -Force (Join-Path $OutDir "uboot_leap_led_device.txt") (Join-Path $driveRoot "uboot_leap_led_device.txt")
    Write-Host "Copied leap_bbb_led_device.bin and uboot_leap_led_device.txt to $driveRoot"
}

Write-Host ""
Write-Host "Build succeeded."
Write-Host "  $Bin"
Write-Host ""
Write-Host "U-Boot:"
Write-Host "  fatload mmc 0:1 0x80000000 leap_bbb_led_device.bin"
Write-Host "  go 0x80000000"
