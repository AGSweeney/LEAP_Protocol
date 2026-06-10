# LeapOS-Gateway

**LeapOS-Gateway** (`leap-eip-gateway.exe`) is the Ethernet/IP bridge product for x86-32 LeapOS.

## Role

| Side | Protocol | Default (single NIC) |
|------|----------|------------------------|
| Plant / PLC | EtherNet/IP (OpENer — planned) | Same port as LEAP, IPv4 |
| Cell | LEAP controller (DISC → MGMT → PD) | BPF EtherType `0x88B6` on `re0`/`em0` |
| Commissioning | HTTP Web UI | `http://<gateway-ip>:8080` |

Single-NIC mode (default): one interface carries IPv4 (E/IP + Web UI) and LEAP raw L2.

## Build

From WSL:

```bash
cd platforms/x86-32/D945GSEJT/LeapOS/rtems-build
bash build-leap-eip-gateway.sh
# or full image with Device + Gateway:
bash build-all.sh all
```

Output: `LeapOS/rtems-image/leap-eip-gateway.exe`

Gateway-only boot image:

```bash
bash build-all.sh iso-gateway
# → LeapOS/rtems-image/leapos-gateway.iso
```

Device and Gateway each have their own ISO and CF image — no combined boot image.

## Configuration

Persisted key/value file (default path `/gateway/config.txt` on CF):

```
network.mode=single
network.ifname=re0
network.ipv4=192.168.1.2
network.mask=255.255.255.0
cyclic_ms=50
mapping.begin=0
mapping.mac=aa:bb:cc:dd:ee:ff
mapping.input.byte=0
mapping.output.byte=2
mapping.status.byte=4
```

## REST API

| Method | Path |
|--------|------|
| GET | `/api/v1/status` |
| GET | `/api/v1/config` |
| PUT | `/api/v1/config` |
| POST | `/api/v1/config/apply` |
| POST | `/api/v1/leap/discover?scan_ms=1000` |
| GET | `/api/v1/leap/peers` |

## Source layout

| Path | Purpose |
|------|---------|
| `src/gateway_init.c` | RTEMS Init, LEAP cyclic loop |
| `src/gateway_http.c` | Web UI + REST |
| `src/gateway_net.c` | NIC + IPv4 bring-up |
| `web/index.html` | Full Web UI (embedded lite copy in firmware) |

Shared transport: [`../LeapPort/src/leap_transport.c`](../LeapPort/src/leap_transport.c)

Bridge logic: [`../../../../leap_core/src/bridge/`](../../../../leap_core/src/bridge/)
