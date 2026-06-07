# LEAP Conformance Studio

Qt 6 Windows app for LEAP commissioning and conformance testing (Npcap).

## Requirements

- Windows 10/11
- Npcap (Administrator for adapter open)
- Qt 6.8.x (Widgets, Svg, Concurrent)
- CMake 3.16+

## Build

Preferred (repo root):

```powershell
.\build.ps1
```

Or manual CMake into local **`build-win/`** (gitignored). Full options:
[docs/BUILD.md](../docs/BUILD.md).

```powershell
cmake -B build-win -DLEAP_BUILD_WIN_L2=ON -DLEAP_BUILD_STUDIO_QT=ON `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64"
cmake --build build-win --config Release --target leap_studio_qt
```

`windeployqt` runs automatically after each `leap_studio_qt` build (Qt DLLs, plugins, MSVC runtime).

To redeploy without rebuilding:

```powershell
cmake --build build-win --config Release --target leap_studio_qt_deploy
# or
powershell -File leap_studio_qt\deploy.ps1
```

## Run

`build-win\leap_studio_qt\Release\leap_studio_qt.exe` — run as **Administrator** when using physical Npcap NICs.

Copy the whole `Release\` folder (exe + Qt DLLs + `platforms\`, etc.) to bench PCs; no separate Qt install required on the target machine.

## I/O Bench Notes

- I/O bench SLO thresholds are defined in `leap_core/inc/leap/conformance/leap_conformance_scenario.h`.
- Current paced-run wire RTT gate uses:
  - `p99 <= 5000 us`
  - `max <= 5000 us` (ceiling gate)
- The I/O Performance tab includes a checkbox:
  - `Enable DIAG polling during soak`
  - When enabled, device DIAG timing rows (`device_reply_last`, `device_reply_worst`)
    are shown.
  - When disabled, those DIAG-specific rows are hidden and soak runs with less
    diagnostics traffic.
