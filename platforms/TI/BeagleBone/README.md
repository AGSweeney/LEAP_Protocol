# BeagleBone Black / Green — LEAP baremetal

AM335x baremetal LEAP device for **BBB** and **BBG** (same binary).

Current deployment model:
- eMMC has U-Boot + FAT partition with `uEnv.txt` and `leap_bbb_led_device.bin`
- normal updates use U-Boot `ums 0 mmc 1` and file copy from host

## Layout

| Path | Purpose |
|------|---------|
| `leap_led_device/` | LEAP runtime source (`leap_bbb_led_device.bin`) |
| `boot/` | Shared `start.S` + `linker.ld` (U-Boot handoff) |
| `build_leap_led_device.ps1` | Build runtime |
| `out/` | Build artifacts (gitignored) |

## Prerequisites

| Component | Default path |
|-----------|----------------|
| StarterWare AM335x | `C:\ti\AM335X_StarterWare_02_00_01_01` |
| GCC arm-none-eabi | `C:\ti\gcc-arm-none-eabi-7-2018-q2-update` |
| Code Composer Studio (gmake) | `C:\ti\ccs2040` |

## Build LEAP runtime

```powershell
powershell -ExecutionPolicy Bypass -File platforms/TI/BeagleBone/build_leap_led_device.ps1
```

Output: `out/leap_led_device/leap_bbb_led_device.bin`

At **U-Boot#** (Ethernet up before `go`):

```text
fatload mmc 0:1 0x80000000 leap_bbb_led_device.bin
go 0x80000000
```

Use the plain **`.bin`**, not `*_ti.bin` / `app` (TI header is for ROM `MLO` only).

## Pinout mapping

### USER LEDs (status only)

| LED | Meaning |
|-----|---------|
| USR0 | Locate/identify blink (5 s on DISC IDENTIFY) |
| USR1 | Connected (owner active) |
| USR2 | Operational state (`LEAP_STATE_OP`) |
| USR3 | Safe/fault (safe state entered or link down) |

### Digital outputs (DO)

| LEAP DO bit | Header pin | SoC GPIO |
|-------------|------------|----------|
| 0 | `P8_19` | `gpio0_22` |
| 1 | `P8_13` | `gpio0_23` |
| 2 | `P8_14` | `gpio0_26` |
| 3 | `P8_17` | `gpio0_27` |
| 4 | `P8_7`  | `gpio2_2` |
| 5 | `P8_8`  | `gpio2_3` |
| 6 | `P8_10` | `gpio2_4` |
| 7 | `P8_9`  | `gpio2_5` |

### Digital inputs (DI)

All DI pins are configured as GPIO inputs with internal pull-up.

| LEAP DI bit | Header pin | SoC GPIO |
|-------------|------------|----------|
| 0 | `P8_12` | `gpio1_12` |
| 1 | `P8_11` | `gpio1_13` |
| 2 | `P8_16` | `gpio1_14` |
| 3 | `P8_15` | `gpio1_15` |
| 4 | `P9_25` | `gpio3_21` |
| 5 | `P9_27` | `gpio3_19` |
| 6 | `P9_29` | `gpio3_15` |
| 7 | `P9_31` | `gpio3_14` |

UART prints `MAC` for `leap_win_discover` / `leap_win_hub` `--peer-mac`.

## Install/update LEAP on eMMC (U-Boot + UMS)

1. Build:
```powershell
powershell -ExecutionPolicy Bypass -File platforms/TI/BeagleBone/build_leap_led_device.ps1
```
2. At U-Boot prompt:
```text
ums 0 mmc 1
```
3. Copy `out/leap_led_device/leap_bbb_led_device.bin` to eMMC FAT root.
4. Ensure eMMC FAT `uEnv.txt` contains:
```text
bootdelay=3
uenvcmd=fatload mmc 1:1 0x80000000 leap_bbb_led_device.bin; go 0x80000000
```
5. Safely remove host drive, press `Ctrl+C` in serial to exit UMS, then reboot.

## Serial (J1)

| Pin | Signal | USB-TTL |
|-----|--------|---------|
| 1 | GND | GND |
| 4 | RX | Adapter TX |
| 5 | TX | Adapter RX |

115200 8N1. Micro-USB serial is **Linux only**; U-Boot and baremetal use **J1**.

## Catch U-Boot

Power without holding BOOT (eMMC). Open J1 **before** power-on. Press **Space** during autoboot.

