# Re-apply LP-AM243 Enet patches after SysConfig regenerates syscfg sources.
param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot "..\LEAP_LP-AM243")
)

$assertOld = @'
    if(gEnetAppSysCfgObj.hEnet[hEnetIndex] == NULL_PTR)
    {
        EnetAppUtils_print("Enet_open failed\r\n");
        EnetAppUtils_assert(NULL_PTR != gEnetAppSysCfgObj.hEnet[hEnetIndex]);
    }
'@

$assertNew = @'
    if(gEnetAppSysCfgObj.hEnet[hEnetIndex] == NULL_PTR)
    {
        EnetAppUtils_print("Enet_open failed\r\n");
        return ENET_EFAIL;
    }
'@

foreach ($cfg in @("Debug", "Release")) {
    $openClose = Join-Path $ProjectRoot "$cfg\syscfg\ti_enet_open_close.c"
    if (Test-Path $openClose) {
        $text = Get-Content $openClose -Raw
        $changed = $false

        if ($text.Contains('EnetAppUtils_assert(NULL_PTR != gEnetAppSysCfgObj.hEnet[hEnetIndex])')) {
            $text = $text.Replace($assertOld.Trim(), $assertNew.Trim())
            $changed = $true
            Write-Host "Patched Enet_open assert in $openClose"
        } elseif ($text.Contains('return ENET_EFAIL;') -and $text.Contains('Enet_open failed')) {
            Write-Host "Enet_open assert already patched: $openClose"
        } else {
            Write-Warning "Enet_open assert pattern not found in $openClose"
        }

        if ($changed) {
            Set-Content -Path $openClose -Value $text -NoNewline
        }
    }

    $boardConfig = Join-Path $ProjectRoot "$cfg\syscfg\ti_board_config.c"
    if (Test-Path $boardConfig) {
        $text = Get-Content $boardConfig -Raw
        $changed = $false

        $ledPattern = '(?s)\.ledMode\s*=\s*\{\s*DP83869_LED_LINKED,\s*/\* Unused \*/\s*DP83869_LED_LINKED_100BTX,\s*DP83869_LED_RXTXACT,\s*DP83869_LED_LINKED_1000BT,\s*\},'
        $ledNew = @'
.ledMode              =
{
    DP83869_LED_LINKED_BLINKACT,
    DP83869_LED_LINKED_BLINKACT,
    DP83869_LED_LINKED_BLINKACT,
    DP83869_LED_LINKED_BLINKACT,
},
'@

        if ([regex]::IsMatch($text, $ledPattern)) {
            $text = [regex]::Replace($text, $ledPattern, $ledNew)
            $changed = $true
            Write-Host "Patched DP83869 LED modes in $boardConfig"
        } elseif ($text.Contains('DP83869_LED_LINKED_BLINKACT')) {
            Write-Host "DP83869 LED modes already patched: $boardConfig"
        } else {
            Write-Warning "DP83869 LED mode pattern not found in $boardConfig"
        }

        $gpio0Old = '.gpio0Mode            = DP83869_GPIO0_RX_SFD,'
        $gpio0New = '.gpio0Mode            = DP83869_GPIO0_LED_GPIO_3,'
        if ($text.Contains($gpio0Old)) {
            $text = $text.Replace($gpio0Old, $gpio0New)
            $changed = $true
            Write-Host "Patched DP83869 GPIO0 mux to LED_GPIO_3 in $boardConfig"
        } elseif ($text.Contains($gpio0New)) {
            Write-Host "DP83869 GPIO0 mux already patched: $boardConfig"
        } else {
            Write-Warning "DP83869 GPIO0 mux pattern not found in $boardConfig"
        }

        if ($changed) {
            Set-Content -Path $boardConfig -Value $text -NoNewline
        }
    }

}
