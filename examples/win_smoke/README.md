# Windows wire smoke (`leap_win_smoke`)

End-to-end LEAP validation over the Npcap loopback adapter (or any Npcap interface).

## What it tests

1. **Bootstrap** — DISC → DIR → MGMT through `OP`, with lease/session/owner checks
2. **PD write** — verifies device `digital_outputs` match the requested value
3. **Cyclic PD** — configurable write cycles with cycle work-time stats (exchange RTT requires separate TX/RX paths; use Linux wire smoke for full exchange latency)
4. **Lease expiry** — short lease, idle without heartbeat, expects device `SAFE` + owner cleared

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

## Usage

```powershell
.\build-win\Release\leap_win_smoke.exe --help
.\build-win\Release\leap_win_smoke.exe
.\build-win\Release\leap_win_smoke.exe --cycles 20 --cycle-ms 40
.\build-win\Release\leap_win_smoke.exe --list
.\build-win\Release\leap_win_smoke.exe \Device\NPF_{your-guid-here}
```

| Option | Default | Description |
|--------|---------|-------------|
| `adapter` | `\Device\NPF_Loopback` | Npcap device name |
| `--cycles N` | 15 | Cyclic PD write count (e.g. `--cycles 100`) |
| `--cycle-ms MS` | 50 | Cycle period (ms) |
| `--outputs HEX` | `0x0015` | Initial PD write value |
| `--skip-cyclic` | off | Skip cyclic phase |
| `--skip-lease-test` | off | Skip lease expiry phase |
| `--no-color` | off | Disable green/red PASS/FAIL output |
| `--list` | | Print adapter hints |
| `--verbose` | | Extra sequence logging |

Or use the CI helper:

```powershell
$env:LEAP_BUILD_DIR = "D:\LEAP_Protocol\build-win"
powershell -ExecutionPolicy Bypass -File tools/ci/wire_smoke_win.ps1
```

## Example success output

```
LEAP Windows wire smoke
adapter: \Device\NPF_Loopback
  MAC: 02:00:4c:4f:4f:50
bootstrap complete - peer 02:00:4c:4f:4f:50
[PASS] bootstrap
bootstrap validation: OP session=1 lease=5000000 us ...
outputs validation: digital_outputs=0x0015
cyclic PD: 15 cycles @ 50 ms (write mode)
PD stats: cycles=15 ok=15 ... overruns=15 ...
note: cycle overruns are expected on Windows Npcap loopback (not real-time scheduling)
[PASS] cyclic PD
...
All validations passed (9/9).
Windows wire smoke: OK (device_frames=...)
```

## Troubleshooting

If open fails, the test prints:

- Driver error text from `pcap_open_live`
- Win32 error code when available
- Checklist (Npcap install, loopback support, Administrator, `--list`)

## Architecture notes

Npcap loopback does not always deliver `pcap_sendpacket` traffic back to capture handles. The Windows transport:

1. Calls `PacketSetLoopbackBehavior(NPF_ENABLE_LOOPBACK)` on open
2. Mirrors transmitted frames through an in-process relay queue for local smoke

The smoke binary runs device + controller cooperatively on one pcap handle (device stack is pumped from controller I/O recv).

## Skip

Set `LEAP_SKIP_WIRE_SMOKE=1` to skip the PowerShell wrapper.
