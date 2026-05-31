# Open LeapDeviceFirmware with sibling libraries linked.
$ErrorActionPreference = "Stop"

& "$PSScriptRoot\import_project_template.ps1" @args

$solution = Join-Path $PSScriptRoot "LeapDeviceFirmware\LeapDeviceFirmware.atsln"
if (-not (Test-Path $solution)) {
    throw "Solution not found: $solution"
}

Write-Host "Opening $solution"

$studioPaths = @(
    "${env:ProgramFiles(x86)}\Atmel\Studio\7.0\atmelstudio.exe",
    "${env:ProgramFiles}\Microchip\Studio\7.0\atmelstudio.exe"
)

foreach ($exe in $studioPaths) {
    if (Test-Path $exe) {
        Start-Process -FilePath $exe -ArgumentList "`"$solution`""
        exit 0
    }
}

Invoke-Item $solution
