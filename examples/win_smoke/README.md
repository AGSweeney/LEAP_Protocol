# Windows wire smoke (`leap_win_smoke`)

End-to-end LEAP bootstrap + PD write over the Npcap **loopback** adapter (`\Device\NPF_Loopback`).

## Requirements

- Windows 10/11 with [Npcap](https://npcap.com/) installed (`wpcap.dll` under `%SystemRoot%\System32\Npcap\`)
- Loopback support enabled in the Npcap installer (default on recent releases)
- CMake + MSVC (Visual Studio 2022 build tools are sufficient)

No Npcap SDK is required to build — the transport loads `wpcap.dll` and `Packet.dll` at runtime.

## Build

```powershell
cmake -S . -B build-win -DLEAP_BUILD_WIN_SMOKE=ON
cmake --build build-win --config Release --target leap_win_smoke
```

## Run

```powershell
.\build-win\Release\leap_win_smoke.exe
# optional adapter override:
.\build-win\Release\leap_win_smoke.exe \Device\NPF_Loopback
```

Or use the CI helper (defaults to `build/` but respects `LEAP_BUILD_DIR`):

```powershell
$env:LEAP_BUILD_DIR = "D:\LEAP_Protocol\build-win"
powershell -ExecutionPolicy Bypass -File tools/ci/wire_smoke_win.ps1
```

Expected success output:

```
LEAP Windows wire smoke (Npcap loopback)
adapter: \Device\NPF_Loopback
  MAC: 02:00:4c:4f:4f:50
bootstrap complete — peer 02:00:4c:4f:4f:50
sent PD WRITE (outputs=0x0015)
Windows wire smoke: OK (device_frames=...)
```

## Architecture notes

Npcap loopback does not always deliver `pcap_sendpacket` traffic back to capture handles on the same machine. The Windows transport therefore:

1. Calls `PacketSetLoopbackBehavior(NPF_ENABLE_LOOPBACK)` when opening the loopback adapter.
2. Mirrors transmitted Ethernet frames through an in-process relay queue so the in-process device stack and controller stack can exchange LEAP frames for local smoke testing.

The smoke binary runs **device + controller cooperatively on one pcap handle** (device stack is pumped from the controller I/O recv path).

## Skip

Set `LEAP_SKIP_WIRE_SMOKE=1` to skip the PowerShell wrapper.
