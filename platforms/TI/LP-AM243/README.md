# LP-AM243 LEAP Device

LEAP device firmware for the [TI LP-AM243 LaunchPad](https://www.ti.com/tool/LP-AM243) (AM2434, ICSSG Ethernet).

## Overview

This port runs the LEAP device stack over **raw Ethernet** (EtherType `0x88B6`) on the MCU+ SDK ICSSG layer-2 example. It replaces the stock echo test with a full LEAP device host:

- **Transport:** ICSSG DMA (no lwIP)
- **RTOS:** FreeRTOS on `r5fss0-0`
- **Profile:** `LEAP_PROFILE_DIGITAL_IO_8X8`
- **PD outputs:** LaunchPad test LEDs (bits 0–3)
- **PD inputs:** User push button on bit 0 (active low)

## Project layout

| Path | Purpose |
| --- | --- |
| `LEAP_LP-AM243/` | Code Composer Studio project (import this folder) |
| `LEAP_LP-AM243/leap_device/` | Board config, GPIO/LED I/O, LEAP host |
| `LEAP_LP-AM243/enet_layer2_icssg.c` | ICSSG bring-up + LEAP frame RX/TX |
| `../../../../leap_core/` | Linked LEAP protocol sources (via `.project`) |

## Prerequisites

- [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO) with AM243x support
- [MCU+ SDK for AM243x](https://www.ti.com/tool/MCU-PLUS-SDK-AM243X) 12.00.00 (matches `example.syscfg`)
- LP-AM243 LaunchPad, Ethernet cable, and a LEAP master on the same L2 segment

## Build and flash

1. Open CCS and **Import** the existing project: `platforms/TI/LP-AM243/LEAP_LP-AM243`
2. Confirm `MCU_PLUS_SDK_PATH` resolves to your SDK install (Project Properties → Resource → Linked Resources).
3. Build the **Debug** or **Release** configuration.
4. Build in CCS — `makefile.targets` runs `platforms/TI/LP-AM243/scripts/patch_enet_lp.ps1` automatically before compiling the Enet driver (single MAC port + no `Enet_open` assert).
5. Connect the LaunchPad (XDS110) and load `LEAP_LP-AM243.out`.
6. If you see `UDMA RX Channel open failed` or `Assertion @ ti_enet_open_close.c:500`, **power-cycle the board** (unplug USB/power for 5 s). CCS reset alone leaves UDMA channels allocated; the old assert makes this worse each debug run.

On first import after a git pull, refresh the project so CCS picks up the linked `leap_core` and `leap_device` source files.

## Runtime

UART0 logs at 115200 baud. **Default: warnings and errors only** (`LEAP_AM243_LOG_*`). Boot banners, MGMT trace, and `PD outputs=` are suppressed unless you build with `-DLEAP_DEVICE_HOST_TRACE_FORCE`. TI Enet bring-up lines (`Open MAC port`, link-up, etc.) still print once at boot via `EnetAppUtils_print`.

During conformance on **U18**, UART stays quiet unless something fails, for example:

```
LEAP PD EXCHANGE_REPLY TX failed ...
LEAP RX queue full (drop=1)
LEAP MGMT rejected msg=...
```

To re-enable full LEAP trace (MGMT, `PD outputs=`, boot banners), add compiler define **`LEAP_DEVICE_HOST_TRACE_FORCE`** to the CCS project.

Connect a LEAP master (e.g. `leap_win_controller` on Windows, or Leap Studio) to the same Ethernet segment. PD digital output bits drive the LaunchPad LEDs:

| Bit | LED |
| --- | --- |
| 0 | TEST_LED1 green (GPIO0_22) — locate blink |
| 1 | TEST_LED2 red (GPIO1_55) |
| 2 | TEST_LED3 red (GPIO0_20) |
| 3 | TEST_LED4 green (GPIO0_84) |

Ethernet connectors (silkscreen **U18** / **U19** on the LaunchPad):

| Jack | ICSSG | Firmware | Status |
| --- | --- | --- | --- |
| **U18** | RGMII2 / MAC port 2 | `icssg1-p2` | **Use this** — LEAP conformance and I/O soak |
| **U19** | RGMII1 / MAC port 1 | `icssg1-p1` | **Do not use** — TX path still unreliable on current firmware |

Connect the LEAP master (Studio or `leap_win_controller`) to **U18** only. Replies egress on the port that received the frame; U19 is left enabled in firmware but is not supported for testing until the CH0 / shared TX pool path is fixed further.

Digital input bit 0 reflects the user push button (GPIO1_54, pressed = 1).

## Master quick test

Build Windows tools with **`.\build.ps1`** from the repo root (local **`build-win/`**,
gitignored — [docs/BUILD.md](../../../docs/BUILD.md)). From a LEAP controller on
the same adapter as the LaunchPad:

```text
leap_win_controller.exe --outputs 0x0001 <adapter>
leap_win_controller.exe --outputs 0x0003 <adapter>
```

## Notes

- The stock ICSSG example interactive menu (statistics, FDB dump, echo) has been removed; the firmware runs as a dedicated LEAP device.
- GPIO and pinmux for the four test LEDs and user button are defined in `example.syscfg` (`LEAP_GPIO_*` instances). `makefile.targets` runs `scripts/patch_enet_lp.ps1` **after** SysConfig regen (single MAC port, no `Enet_open` assert, PHY reset deassert in `GPIO_init`). Power-cycle the board after any UDMA/assert failure before retesting.
- Product code: `0x0243A243`, firmware revision `1`.

## See also

- [Host build (CMake / Windows tools)](../../../docs/BUILD.md)
- [TI platforms index](../README.md)
- [BeagleBone LEAP device](../BeagleBone/README.md) — similar raw-Ethernet pattern on AM335x
