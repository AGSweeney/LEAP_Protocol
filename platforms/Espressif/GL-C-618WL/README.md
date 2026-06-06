# LEAP Device — Gledopto GL-C-618WL

ESP-IDF LEAP device for the **Gledopto GL-C-618WL** (Elite 4D-EXMU) ESP32 LED
controller. Ethernet uses the on-board **LAN8720** PHY; LEAP frames are handled
at L2 via a custom lwIP EtherType hook.

## Exposed hardware

What is actually on the enclosure — the only user-wirable I/O:

| Terminal / port | MCU pin | Role |
| --- | ---: | --- |
| RJ45 | RMII | Ethernet (LEAP transport) |
| DC power | — | 5–24 V input |
| LED data CH0 | GPIO16 | 300× 5050 SMD addressable (WS2812 timing) |
| LED data CH1 | GPIO2 | WS2812 / SK6812 strip (optional second run) |

### Power and long strips

The enclosure **DC input** is rated **5–24 V** for real LED loads. **USB is only for
flash / serial** — typical USB (0.5–1 A, sometimes 3 A on a good port) cannot feed
a **300× 5050** run. Expect brownout reboots if the master lights the strip on USB
alone (`reset reason: brownout`, serial drops).

| Bench power | Strip | Expect |
| --- | --- | --- |
| USB only | unplugged | LEAP + Ethernet OK |
| USB only | 300 px connected | Reboot when outputs turn on |
| DC 5–24 V (rated) + strip 5 V at terminals | connected | Normal operation |

- **USB dev setup** — leave strip data/power off the terminals; validate LEAP with
  `leap_win_discover` / `leap_win_controller --cyclic`.
- **With strip** — use the **DC jack** for the controller and a **separate 5 V**
  supply at the LED terminals (10 A+ class for 300 px at full drive). Common ground.
- Firmware caps pixel level at `LEAP_LED_MAX_LEVEL` (`board_config.h`, default **0x40**)
  as a safety net; that still overloads USB on a long run — it is not a substitute for
  proper strip power.

## Internal only (not LEAP PD I/O)

These MCU pins exist on the PCB but are **not** screw-terminal GPIO:

| Pin | Role |
| ---: | --- |
| GPIO18 | Relay — energizes LED power rail at boot |
| GPIO17 | On-board function button |
| GPIO13 | Internal signal port (no external terminal) |
| GPIO0/5/23/33 + RMII | Ethernet PHY |

## LEAP digital I/O map

PD maps only to the two exposed LED data terminals. There are no exposed digital
inputs on this controller.

| PD output bit | Function |
| ---: | --- |
| 0 | CH0 (GPIO16) red fill |
| 1 | CH0 green fill |
| 2 | CH0 blue fill |
| 3 | CH0 white fill |
| 4 | CH1 (GPIO2) red fill |
| 5 | CH1 green fill |
| 6 | CH1 blue fill |
| 7 | CH1 white fill |

| PD input bit | Function |
| ---: | --- |
| 0–7 | Not exposed — always 0 |

Strips stay **off** until the LEAP master applies PD outputs in **OP**. On SAFE
/ release they turn off again. **IDENTIFY** returns identity only. **LOCATE_DEVICE**
drives the internal status LED on **GPIO13** (dumb LED, not WS2812) per
duration/pattern. Locate never changes PD outputs.

### Bench setup (this workstation)

Gledopto RJ45 is on **Ethernet 3** (Intel I225-LM, host MAC `38:7c:76:d9:8a:b8`).

Npcap adapter for LEAP tools:

```text
\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}
```

Device MAC: `94:51:dc:21:f0:2f`

### Discovery and controller

Build Windows tools from repo root (`.\build.ps1` or CMake → **`build-win/`**, gitignored).
See [docs/BUILD.md](../../docs/BUILD.md).

```powershell
# Discovery (expect peer 94:51:dc:21:f0:2f)
build-win\Release\leap_win_discover.exe --scan-ms 5000 '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'

# Identity (no LED; releases stale OP session first by default)
build-win\Release\leap_win_identify.exe --peer-mac 94:51:dc:21:f0:2f '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'

# Locate device (GPIO13 status LED)
build-win\Release\leap_win_identify.exe --peer-mac 94:51:dc:21:f0:2f --locate --duration-ms 5000 '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'

# Sustained control — keep this running; Ctrl+C to release (strips go off)
build-win\Release\leap_win_controller.exe --cyclic --outputs 0x0001 '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'   # CH0 red
build-win\Release\leap_win_controller.exe --cyclic --outputs 0x0007 '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'   # CH0 RGB
build-win\Release\leap_win_controller.exe --cyclic --outputs 0x0008 '\Device\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'   # CH0 white

# One-shot only flashes briefly (bootstrap writes then session release)
# build-win\Release\leap_win_controller.exe --outputs 0x0007 '...'
```

### Automated test script (USB bench, strip unplugged)

Runs discovery, bootstrap, PD, exchange, DIAG, and lease-demo in one pass (~2 min):

```powershell
# Strip unplugged on USB; Ctrl+C during 5s pause if LEDs are still connected
powershell -ExecutionPolicy Bypass -File platforms\Espressif\GL-C-618WL\test_leap_bench.ps1

# Skip pause when strip is off or proper DC + strip 5V is wired
powershell -ExecutionPolicy Bypass -File platforms\Espressif\GL-C-618WL\test_leap_bench.ps1 -Force
```

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
  installed and sourced

## Build and flash

```bash
cd platforms/Espressif/GL-C-618WL
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

## Project layout

```
GL-C-618WL/
├── components/leap_core/   # device-side LEAP stack
└── main/
    ├── board_config.h      # exposed vs internal pin definitions
    ├── eth_init.c          # LAN8720 bring-up
    ├── led_output.c        # dual WS2812 RMT driver
    ├── leap_hw.c           # relay boot + strip PD mapping
    ├── leap_eth.c          # lwIP hook + LEAP TX
    └── leap_host.c         # leap_device_stack integration
```

## Notes

- DHCP runs on Ethernet for serial logging convenience. LEAP uses L2 only.
- The relay is turned on at boot to power the LED outputs; it is not a LEAP DO.
