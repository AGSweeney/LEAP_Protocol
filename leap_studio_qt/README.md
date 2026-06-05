# LEAP Conformance Studio

Qt 6 Windows app for LEAP commissioning and conformance testing (Npcap).

## Requirements

- Windows 10/11
- Npcap (Administrator for adapter open)
- Qt 6.8.x (Widgets, Svg, Concurrent)
- CMake 3.16+

## Build

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
