# LEAP Device — Waveshare ESP32-P4-WIFI6-POE-ETH

ESP-IDF LEAP device for the [Waveshare ESP32-P4-WIFI6-POE-ETH](https://docs.waveshare.com/ESP32-P4-WIFI6-POE-ETH)
board. Ethernet uses the on-board **IP101** PHY with external RMII REF clock on
GPIO50; LEAP frames are handled at L2 via a custom lwIP EtherType hook.

Hardware and ESP-IDF settings are aligned with the CIPDataGateway reference
(`ENIP_Devices/CIPDataGateway`). LEAP stack integration follows the KC868-A16
and GL-C-618WL Espressif ports.

## Exposed hardware

| Terminal / bus | Role |
| --- | --- |
| RJ45 | Ethernet (LEAP transport, optional PoE power) |
| 40-pin header OUT1–OUT8 | 8 digital outputs (GPIO) |
| 40-pin header IN1–IN8 | 8 digital inputs (GPIO, pull-up) |
| H2 OUT_LE | Locate / identify LED (GPIO38) |

The fixed **power indicator LED** is wired to 5 V and is not software
controllable. Use **GPIO38 (OUT_LE)** for `LOCATE_DEVICE`.

### Ethernet (IP101)

| Signal | GPIO |
| ---: | ---: |
| MDC | 31 |
| MDIO | 52 |
| REF CLK (in) | 50 |
| PHY reset / power | 51 |
| PHY addr | 1 |

### Reserved (not LEAP PD I/O)

| GPIO | Role |
| ---: | --- |
| 7, 8 | I2C header (SDA/SCL) |
| 9–13 | I2S / audio |
| 39–44 | SD card (SDIO) |
| 53 | PA amplifier enable |
| 0, 1 | Strapping / boot |

## LEAP digital I/O map

Profile: **8×8 digital I/O** (`LEAP_PROFILE_DIGITAL_IO_8X8`).

| PD output bit | Header | GPIO |
| ---: | --- | ---: |
| 0 | OUT1 | 2 |
| 1 | OUT2 | 3 |
| 2 | OUT3 | 4 |
| 3 | OUT4 | 5 |
| 4 | OUT5 | 6 |
| 5 | OUT6 | 20 |
| 6 | OUT7 | 21 |
| 7 | OUT8 | 22 |

| PD input bit | Header | GPIO |
| ---: | --- | ---: |
| 0 | IN1 | 23 |
| 1 | IN2 | 24 |
| 2 | IN3 | 25 |
| 3 | IN4 | 26 |
| 4 | IN5 | 27 |
| 5 | IN6 | 32 |
| 6 | IN7 | 33 |
| 7 | IN8 | 36 |

Outputs stay **low** until the LEAP master applies PD outputs in **OP**. On SAFE
or lease release they return to the safe value (off). **IDENTIFY** returns
identity only. **LOCATE_DEVICE** blinks GPIO38.

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/)
  with `esp32p4` target support, installed and sourced

## Build and flash

```bash
cd platforms/Espressif/ESP32-P4
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash monitor
```

Or on Windows after sourcing ESP-IDF:

```powershell
powershell -ExecutionPolicy Bypass -File platforms\Espressif\ESP32-P4\build_leap_device.ps1
powershell -ExecutionPolicy Bypass -File platforms\Espressif\ESP32-P4\build_leap_device.ps1 -Port COM3 -Flash -Monitor
```

Flash via the **Type-C UART** port on the board (not the Type-A USB OTG port).

## Project layout

```
ESP32-P4/
├── components/
│   └── leap_core/      # device-side LEAP stack (repo leap_core/)
└── main/
    ├── board_config.h  # pinout and PD bit map
    ├── eth_init.c      # IP101 bring-up (external REF clock GPIO50)
    ├── leap_hw.c       # GPIO PD mapping + locate LED
    ├── leap_eth.c      # lwIP hook + LEAP TX
    └── leap_host.c     # leap_device_stack integration
```

## Conformance (LEAP Conformance Studio)

1. Open the bench NIC as Administrator.
2. **Discovery** tab → **Scan for peers (3s)**. Copy the **MAC** column into
   **Connection → Expected peer MAC**.
3. **Conformance** tab → **Run all steps**.

Use the boot log `identity: primary_mac=` for `--peer-mac`. Product code on the
wire: `0x0450E601`.

## Notes

- No DHCP — Ethernet is L2-only for LEAP transport (link-local IPv4 satisfies lwIP).
- 32 MB flash / PSRAM defaults match the Waveshare board (HEX PSRAM @ 200 MHz).
- Wi-Fi 6 (ESP32-C6 co-processor) is not used by this LEAP port; traffic is
  wired Ethernet only.

### Performance (sdkconfig.defaults)

LEAP uses raw L2 Ethernet, not TCP. IRAM options target RX/TX latency on the
EMAC and lwIP hook path:

| Option | Purpose |
| --- | --- |
| `CONFIG_ETH_IRAM_OPTIMIZATION` | EMAC driver RX/TX functions in IRAM |
| `CONFIG_LWIP_IRAM_OPTIMIZATION` | lwIP RX/TX helpers in IRAM (EtherType hook, `ethernet_output`) |
| `CONFIG_LWIP_TCPIP_CORE_LOCKING` | Safe lwIP TX from the LEAP task on CPU1 |
| `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0` | tcpip thread pinned away from LEAP task (CPU1) |

`CONFIG_LWIP_EXTRA_IRAM_OPTIMIZATION` (TCP-only) is intentionally **not** set.

After changing `sdkconfig.defaults`, reconfigure so local `sdkconfig` picks up
PSRAM speed and new options:

```bash
idf.py fullclean reconfigure build
```
