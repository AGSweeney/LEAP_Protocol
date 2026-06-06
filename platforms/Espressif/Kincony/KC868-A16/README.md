# LEAP Device — Kincony KC868-A16

ESP-IDF LEAP device for the **Kincony KC868-A16** Ethernet I/O controller.
Ethernet uses the on-board **LAN8720A** PHY with RMII clock output on GPIO17;
LEAP frames are handled at L2 via a custom lwIP EtherType hook.

Hardware details are derived from the OpENer KC868-A16 reference port
(`ENIP_Devices/OpENer_EnIP_KC868A16`).

## Exposed hardware

| Terminal / bus | Role |
| --- | --- |
| RJ45 | Ethernet (LEAP transport) |
| Y01–Y16 | 16 relay outputs (PCF8574, active-low) |
| X01–X16 | 16 opto-isolated digital inputs (PCF8574, active-low) |
| A1–A4 | 4 analog inputs (not mapped to LEAP PD in this port yet) |

### Ethernet (LAN8720A)

| Signal | GPIO |
| ---: | ---: |
| MDC | 23 |
| MDIO | 18 |
| TXD0 | 19 |
| TXD1 | 22 |
| TX_EN | 21 |
| RXD0 | 25 |
| RXD1 | 26 |
| RX_DV | 27 |
| REF CLK (out) | 17 |
| PHY addr | 0 |

`RST_NET` is tied to 3.3 V via RC — no ESP32 GPIO reset line.

### I2C (PCF8574 expanders)

| Signal | GPIO |
| ---: | ---: |
| SDA | 4 |
| SCL | 5 |

| Role | I2C address |
| --- | ---: |
| Inputs X01–X08 | 0x22 |
| Inputs X09–X16 | 0x21 |
| Outputs Y01–Y08 | 0x24 |
| Outputs Y09–Y16 | 0x25 |

### Internal only (not LEAP PD I/O)

| Pin | Role |
| ---: | --- |
| GPIO13 / GPIO16 | RS485 UART (9600 baud) |
| GPIO2 | IR receiver |
| GPIO15 | IR transmitter (used for LOCATE_DEVICE) |
| GPIO32, GPIO33, GPIO14 | HT1–HT3 local inputs |

## LEAP digital I/O map

PD maps to the 16 relay outputs and 16 digital inputs on the screw terminals.

| PD output bit | Relay |
| ---: | --- |
| 0–7 | Y01–Y08 |
| 8–15 | Y09–Y16 |

| PD input bit | Input |
| ---: | --- |
| 0–7 | X01–X08 |
| 8–15 | X09–X16 |

Relays stay **off** until the LEAP master applies PD outputs in **OP**. On SAFE
or lease release they turn off again. **IDENTIFY** returns identity only.
**LOCATE_DEVICE** blinks the on-board IR transmitter (GPIO15).

Analog inputs A1–A4 and local HT inputs are not exposed on the LEAP PD channel
yet (16-bit digital I/O only in the current stack).

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
  installed and sourced

## Build and flash

```bash
cd platforms/Espressif/Kincony/KC868-A16
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Or on Windows after sourcing ESP-IDF:

```powershell
powershell -ExecutionPolicy Bypass -File platforms\Espressif\Kincony\KC868-A16\build_leap_device.ps1
powershell -ExecutionPolicy Bypass -File platforms\Espressif\Kincony\KC868-A16\build_leap_device.ps1 -Port COM3 -Flash -Monitor
```

## Project layout

```
KC868-A16/
├── components/
│   ├── leap_core/      # device-side LEAP stack
│   ├── i2c_manager/    # shared I2C bus
│   └── pcf8574/          # I/O expander driver
└── main/
    ├── board_config.h    # pinout and PD bit map
    ├── eth_init.c        # LAN8720 bring-up (CLK out GPIO17)
    ├── leap_hw.c         # relay + input PD mapping
    ├── leap_eth.c        # lwIP hook + LEAP TX
    └── leap_host.c       # leap_device_stack integration
```

## Conformance (LEAP Conformance Studio)

1. Open the bench NIC (e.g. Ethernet 3) as Administrator.
2. **Discovery** tab → **Scan for peers (3s)**. Copy the **MAC** column into
   **Connection → Expected peer MAC** (Studio auto-fills when exactly one peer
   is found).
3. **Conformance** tab → **Run all steps**. Studio probes IDENTIFY + LEAP-DIR
   (`PROFILE_REPLY` / `READ_DIRECTORY`) and runs tests only from the endpoint
   descriptors the device reports (no product-specific scenario).

If **DISC peer MAC** fails while **peers=1** passes, the Expected peer MAC does
not match the discovered device (a stale GL-C default or WiFi label MAC is a
common cause). `38:7c:76:…` in device logs is the **controller** HELLO source,
not the KC868 address.

## Notes

- No DHCP or IPv4 configuration — Ethernet is L2-only for LEAP transport.
- Use the boot log `identity: primary_mac=` (same source as GL-C-618WL) for
  `--peer-mac` / conformance. That MAC comes from the Ethernet driver netif and
  is the source address on HELLO_REPLY frames.
- Product code on the wire: `0x0868A016`.
- SDK defaults match the OpENer port: RMII clock **output** on GPIO17, PHY address 0.
