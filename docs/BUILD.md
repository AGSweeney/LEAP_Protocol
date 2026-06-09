# Building LEAP Protocol

All CMake output lives in **local `build*` directories at the repo root**. These
folders are **gitignored** — never commit them. Regenerate any time with the
commands below.

| Directory | Toolchain | Created by |
| --- | --- | --- |
| `build/` | Linux / macOS GCC or Clang | `cmake -S . -B build` |
| `build-win/` | Windows MSVC (multi-config) | `.\build.ps1` or `cmake -B build-win` |
| `build-wsl/` | WSL Linux (optional local) | `cmake -S . -B build-wsl` |
| `build-wsl-ci/` | WSL CI / build scratch | ad hoc; safe to delete |

## Clean workspace

```powershell
# Windows — remove MSVC tree and reconfigure
.\build.ps1 -Clean

# Or delete all local build trees manually
Remove-Item -Recurse -Force build, build-win, build-wsl, build-wsl-ci -ErrorAction SilentlyContinue
```

```bash
# Linux
rm -rf build build-wsl build-wsl-ci
```

Embedded targets (ClearCore, ESP-IDF, CCS) build **inside their platform
projects** — not in these root `build*` folders. See each platform README.

---

## Linux (native)

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Binaries: `./build/leap_tests`, `./build/leap_linux_device`, etc.

Wire examples need **native Linux** (not WSL2 `AF_PACKET`). See
[examples/linux_loopback/README.md](../examples/linux_loopback/README.md).

---

## Windows (recommended)

From repo root, with Visual Studio 2022 + CMake tools + Qt 6 (for Studio):

```powershell
.\build.ps1                 # Release: Studio, conformance CLI, tests, win_l2 tools
.\build.ps1 -Configure      # force CMake reconfigure
.\build.ps1 -Clean          # delete build-win and configure fresh
.\build.ps1 -Test           # build + run leap_tests
.\build.ps1 -Target leap_win_controller
```

Default output tree: **`build-win/`**

| Artifact | Path |
| --- | --- |
| Unit tests | `build-win\Release\leap_tests.exe` |
| Conformance CLI | `build-win\leap_cli\Release\leap_conformance.exe` |
| Studio | `build-win\leap_studio_qt\Release\leap_studio_qt.exe` |
| Npcap controller | `build-win\Release\leap_win_controller.exe` |

Manual CMake (same tree):

```powershell
cmake -S . -B build-win -DLEAP_BUILD_WIN_L2=ON -DLEAP_BUILD_STUDIO_QT=ON `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64"
cmake --build build-win --config Release
```

Optional smoke binary: add `-DLEAP_BUILD_WIN_SMOKE=ON`.

Override build directory for scripts: `$env:LEAP_BUILD_DIR = "D:\path\build-win"`.

---

## WSL (unit tests only)

From **Windows PowerShell**, use the WSL helper (recommended):

```powershell
.\build-wsl.ps1 -Test       # configure + build + ctest in WSL → build-wsl\
.\build-wsl.ps1 -Clean      # delete build-wsl and reconfigure
```

Or from inside WSL:

```bash
cmake -S . -B build-wsl
cmake --build build-wsl -j
ctest --test-dir build-wsl --output-on-failure
```

WSL can compile and run **`ctest`**, but not AF_PACKET wire examples.

---

## CI

GitHub Actions uses a fresh **`build/`** directory on Linux and **`build/`** on
Windows (see [.github/workflows/ci.yml](../.github/workflows/ci.yml)). CI does
not build Npcap examples on Windows runners.

---

## Related docs

| Topic | Doc |
| --- | --- |
| Unit tests | [tests/README.md](../tests/README.md) |
| Linux wire examples | [examples/linux_loopback/README.md](../examples/linux_loopback/README.md) |
| Windows Npcap examples | [leap_cli/win_l2/README.md](../leap_cli/win_l2/README.md) |
| Conformance Studio | [leap_studio_qt/README.md](../leap_studio_qt/README.md) |
| ClearCore firmware | [platforms/clearcore/README.md](../platforms/clearcore/README.md) |
| LP-AM243 (CCS) | [platforms/TI/LP-AM243/README.md](../platforms/TI/LP-AM243/README.md) |
