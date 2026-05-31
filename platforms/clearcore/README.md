# ClearCore LEAP device host

Firmware lives in `LeapDeviceFirmware/`. **libClearCore**, **LwIP**, **ClearLinkCompatibilityFirmware**, and **Tools** are linked as siblings under `platforms/clearcore/` (same layout as ProjectTemplate).

## Setup (once)

From `LEAP_Protocol`:

```powershell
powershell -ExecutionPolicy Bypass -File platforms/clearcore/import_project_template.ps1
```

Uses `D:\ClearCore-Motion-Stack\ProjectTemplate` when present; otherwise clones into `vendor/`. Override:

```powershell
powershell -ExecutionPolicy Bypass -File platforms/clearcore/import_project_template.ps1 -MotionStackRoot D:\ClearCore-Motion-Stack
```

## Open Microchip Studio

```powershell
powershell -ExecutionPolicy Bypass -File platforms/clearcore/open_studio.ps1
```

Or open:

`platforms/clearcore/LeapDeviceFirmware/LeapDeviceFirmware.atsln`

After import, Solution Explorer should show **ClearCore**, **LwIP**, and **LeapDeviceFirmware** — not "(unavailable)".

## Build order (Release)

1. ClearCore  
2. LwIP  
3. LeapDeviceFirmware  

## Test with PC controller

```powershell
$adp = '\Device\NPF_{your-nic-guid}'
.\build-win\Release\leap_win_controller.exe $adp
```

ClearCore USB serial logs LEAP bootstrap and PD.
