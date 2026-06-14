# LeapOS-Gateway — Raspberry Pi 4

**LeapOS-Gateway** is the Ethernet/IP bridge product for LEAP cell ↔ plant traffic.

This Pi 4 port mirrors the x86-32 gateway role. Implementation is staged:

| Stage | Delivery |
| --- | --- |
| **Now (scaffold)** | Alpine aarch64 SD image, stub `leap-gateway` daemon, static LEAP CLI tools |
| **Next** | Port gateway from [x86-32 LeapGateway/src](../../x86-32/D945GSEJT/LeapGateway/src/) (Alpine Linux reference) |
| **Later** | Web UI + REST (`web/index.html` shared with x86) |

## Role

| Side | Protocol | Default (single NIC) |
| --- | --- | --- |
| Plant / PLC | EtherNet/IP (OpENer — planned) | Same port as LEAP, IPv4 |
| Cell | LEAP controller (DISC → MGMT → PD) | AF_PACKET EtherType `0x88B6` on `eth0` |
| Commissioning | HTTP Web UI | `http://<gateway-ip>:8080` (planned) |

Single-NIC mode (default): one interface carries IPv4 (E/IP + Web UI) and LEAP raw L2.

## Build (Alpine image today)

```bash
cd ../LeapGateway-linux/alpine
sudo bash mk-image.sh
```

See [../README.md](../README.md) for flash and serial console details.

## Configuration

Default path `/cf/config.txt` — same schema as the x86 gateway. Up to **16** mapping
slots when the full bridge binary lands.

## Source layout (planned native build)

| Path | Purpose |
| --- | --- |
| `src/` | *(future)* Pi4 gateway daemon — start from x86 `LeapGateway/src/` |
| `../LeapGateway-linux/alpine/overlay/` | Image overlay (OpenRC, stub daemon, config) |

Shared stack: [`../../../../leap_core/`](../../../../leap_core/)

Bridge logic: [`../../../../leap_core/src/bridge/`](../../../../leap_core/src/bridge/)
