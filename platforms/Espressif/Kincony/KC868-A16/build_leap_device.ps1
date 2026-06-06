# Build and optionally flash the KC868-A16 LEAP device (ESP-IDF).
param(
    [string]$Port = "",
    [switch]$Flash,
    [switch]$Monitor
)

$ErrorActionPreference = "Stop"

if (-not $env:IDF_PATH) {
    throw "IDF_PATH is not set. Source export.ps1 from your ESP-IDF install first."
}

$ProjectDir = $PSScriptRoot
Push-Location $ProjectDir
try {
    if (-not (Test-Path "sdkconfig")) {
        & idf.py set-target esp32
        if ($LASTEXITCODE -ne 0) { throw "idf.py set-target failed" }
    }

    & idf.py build
    if ($LASTEXITCODE -ne 0) { throw "idf.py build failed" }

    if ($Flash -or $Port -ne "") {
        if ($Port -eq "") {
            throw "Specify -Port COMx when flashing"
        }
        $flashArgs = @("-p", $Port, "flash")
        if ($Monitor) {
            $flashArgs += "monitor"
        }
        & idf.py @flashArgs
        if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed" }
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Build succeeded: $ProjectDir\build\leap_kc868_a16.bin"
