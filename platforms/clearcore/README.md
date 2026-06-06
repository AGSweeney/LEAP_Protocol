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

`leap_protocol/` is a junction to the LEAP repo root. Microchip Studio include paths and sources use `leap_protocol/leap_core/inc` and `leap_protocol/leap_core/src`. After changing `LeapDeviceFirmware.cppproj`, rebuild **LeapDeviceFirmware** once in Studio so `Release/Makefile` is regenerated (or run `build_leap_device.ps1`, which patches stale Makefiles automatically).

## Test with PC controller

On the same LAN segment (Linux or Windows):

Host LEAP tools are built at the repo root into local **`build/`** or **`build-win/`**
(gitignored). See [docs/BUILD.md](../../docs/BUILD.md).

```bash
# Linux
sudo ./build/leap_linux_controller eth0
```

```powershell
# Windows
$adp = '\Device\NPF_{your-nic-guid}'
.\build-win\Release\leap_win_controller.exe $adp
```

ClearCore USB serial logs LEAP bootstrap and PD.
