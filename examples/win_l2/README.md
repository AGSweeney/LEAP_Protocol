# Windows L2 examples (`leap_win_device` + `leap_win_controller`)

Two-process LEAP over Npcap — mirrors `examples/linux_loopback/` for real adapters.

## Build

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build-win -DLEAP_BUILD_WIN_L2=ON
& $cmake --build build-win --config Release --target leap_win_device leap_win_controller leap_win_discover leap_win_hub
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

## Multi-device (ClearCore bench)

Use a switch so both devices share L2 with the controller NIC. Flash the same LEAP firmware on each ClearCore (unique factory MACs).

**Default hub bring-up** uses a **1 s** broadcast scan with **early exit** once `--min-peers` are found, and **1 s** bootstrap recv timeout (was 3 s scan + 5 s recv).

**Step 1 — verify discovery (no bootstrap)**

```powershell
$adp = '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'
.\build-win\Release\leap_win_discover.exe --scan-ms 1000 --min-peers 2 --promisc $adp
```

Expect two peer MAC lines. Discovery stops as soon as `min-peers` are seen.

**Step 2 — session hub (discover → bootstrap all → round-robin PD)**

```powershell
.\build-win\Release\leap_win_hub.exe `
  --min-peers 2 `
  --exchange `
  --cyclic-ms 100 `
  --promisc `
  --stats-interval 500 `
  $adp
```

**Fast re-run with known MACs (skip broadcast scan)**

```powershell
.\build-win\Release\leap_win_hub.exe `
  --scan-ms 0 `
  --peer-mac 24:15:10:b0:61:57 `
  --peer-mac 24:15:10:b0:5f:bc `
  --min-peers 2 `
  --exchange `
  --promisc `
  $adp
```

| Option | Meaning |
|--------|---------|
| `--min-peers 2` | Fail unless N peers found; **stop scan early** at N |
| `--scan-ms 0` | With `--peer-mac`: unicast probe only; without: 250 ms minimal broadcast |
| `--peer-mac` | Known device MAC (repeatable); fastest repeat runs |
| `--pacing` (default) | Sleep `cyclic-ms` after each peer's cycle |
| `--no-pacing` | Full-speed round-robin (Linux hub style) |

With **2 peers** and `--cyclic-ms 100 --pacing`, each device gets a PD cycle roughly every **200 ms** (one lap visits both peers, sleeping 100 ms after each).

Only **one controller** (`leap_win_hub` or `leap_win_controller`) should run on the segment. Stop other LEAP controllers before starting the hub.

## vs loopback smoke

| Program | Model |
|---------|--------|
| `leap_win_smoke` | Single process, in-process device + relay (loopback only) |
| `leap_win_device` + `leap_win_controller` | Two processes, real L2 (physical NIC or loopback if Npcap delivers between handles) |
| `leap_win_discover` | HELLO scan only — peer MAC list |
| `leap_win_hub` | Multi-peer discover → bootstrap → round-robin PD |

For local CI-style validation without a second process, keep using `leap_win_smoke` on `\Device\NPF_Loopback`.

## Controller options

Same as Linux `leap_linux_controller`: `--cyclic`, `--cyclic-ms`, `--exchange`, `--lease-demo`, `--diag`, `--promisc`, `--stats`, `--list`.
