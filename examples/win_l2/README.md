# Windows L2 examples (`leap_win_device` + `leap_win_controller`)

Two-process LEAP over Npcap — mirrors `examples/linux_loopback/` for real adapters.

## Build

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build-win -DLEAP_BUILD_WIN_L2=ON
& $cmake --build build-win --config Release --target leap_win_device leap_win_controller
```

Run **both** programs as Administrator when using a physical NIC.

## Find your adapter

```powershell
.\build-win\Release\leap_win_controller.exe --list
Get-NetAdapter | Format-Table Name, InterfaceGuid, MacAddress, Status
```

Npcap name form: `\Device\NPF_{InterfaceGuid}` (include braces).

Example (Mellanox 10G):

```powershell
$adp = '\Device\NPF_{6350838F-D1D5-407E-874E-8EBF642EE1DE}'
```

## Same PC vs two machines

Each process needs its **own Ethernet MAC**. Opening the same Mellanox adapter in both `leap_win_device` and `leap_win_controller` on one PC gives both stacks the same MAC, so bootstrap will not work.

| Setup | Works |
|-------|--------|
| Controller on PC, device on **another host** on the LAN | Yes |
| Both on same PC, same physical adapter | No (duplicate MAC) |
| Local single-process validation | Use `leap_win_smoke` on `\Device\NPF_Loopback` |

## Run (two machines or device on LAN)

**Terminal 1 — device**

```powershell
.\build-win\Release\leap_win_device.exe $adp
```

**Terminal 2 — controller**

```powershell
.\build-win\Release\leap_win_controller.exe $adp
.\build-win\Release\leap_win_controller.exe $adp --cyclic --cyclic-ms 50
.\build-win\Release\leap_win_controller.exe $adp --diag
```

## vs loopback smoke

| Program | Model |
|---------|--------|
| `leap_win_smoke` | Single process, in-process device + relay (loopback only) |
| `leap_win_device` + `leap_win_controller` | Two processes, real L2 (physical NIC or loopback if Npcap delivers between handles) |

For local CI-style validation without a second process, keep using `leap_win_smoke` on `\Device\NPF_Loopback`.

## Controller options

Same as Linux `leap_linux_controller`: `--cyclic`, `--cyclic-ms`, `--exchange`, `--lease-demo`, `--diag`, `--promisc`, `--stats`, `--list`.
