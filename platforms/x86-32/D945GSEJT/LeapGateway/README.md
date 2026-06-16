# LeapGateway — shared application sources

Shared LEAP gateway application logic (HTTP Web UI, LEAP session hub, process
data I/O, OpENer bridge). The **shipping gateway product** is Alpine Linux i386
under [`../LeapGateway-linux/`](../LeapGateway-linux/).

## Role

| Side | Protocol | Default (single NIC) |
|------|----------|------------------------|
| Plant / PLC | EtherNet/IP (OpENer) | Same port as LEAP, IPv4 |
| Cell | LEAP controller (DISC → MGMT → PD) | BPF EtherType `0x88B6` |
| Commissioning | HTTP Web UI | `http://<gateway-ip>:8080` |

Single-NIC mode (default): one interface carries IPv4 (E/IP + Web UI) and LEAP raw L2.

## Build (Alpine gateway)

From WSL:

```bash
cd platforms/x86-32/D945GSEJT/LeapGateway-linux
bash build-leap-gateway.sh
sudo bash alpine/mk-image.sh
```

Output: `LeapOS/rtems-image/leapos-gateway-alpine.img`

See [`../LeapGateway-linux/alpine/README.md`](../LeapGateway-linux/alpine/README.md)
for QEMU boot and image details.

## Configuration

Persisted key/value file (default path `/cf/config.txt` on the Alpine image):

```
network.mode=single
network.ifname=eth0
network.ipv4=192.168.1.2
network.mask=255.255.255.0
cyclic_ms=50
mapping.begin=0
mapping.mac=aa:bb:cc:dd:ee:ff
mapping.input.byte=0
mapping.output.byte=2
mapping.status.byte=4
mapping.begin=1
mapping.mac=11:22:33:44:55:66
mapping.input.byte=8
mapping.output.byte=10
mapping.status.byte=12
```

Up to **16** mapping slots (`mapping.begin=0` … `15`). **Connect LEAP** bootstraps every enabled slot to OP and runs cyclic PD for each peer (round-robin per gateway tick). Each slot has its own E/IP byte offsets in the assembly image.

## REST API

| Method | Path |
|--------|------|
| GET | `/api/v1/status` |
| GET | `/api/v1/config` |
| PUT | `/api/v1/config` |
| POST | `/api/v1/config/apply` |
| POST | `/api/v1/leap/discover` (3 s scan; skipped while owner sessions active) |
| POST | `/api/v1/leap/disconnect` |
| POST | `/api/v1/leap/connect` |
| GET | `/api/v1/leap/peers` |

## Source layout

| Path | Purpose |
|------|---------|
| `src/gateway_http.c` | Web UI + REST |
| `src/gateway_leap_session.c` | LEAP controller session hub |
| `src/gateway_pd_io.c` | Process data exchange |
| `src/gateway_global.c` | Runtime state + config reload |
| `web/index.html` | Full Web UI (embedded in firmware) |

Platform layer (Linux): [`../LeapGateway-linux/src/`](../LeapGateway-linux/src/)

Shared transport: [`../LeapPort/src/leap_transport.c`](../LeapPort/src/leap_transport.c)

Bridge logic: [`../../../../leap_core/src/bridge/`](../../../../leap_core/src/bridge/)

## RTEMS device port

The LEAP **device** firmware (`leap-port.exe`) is built from [`../LeapPort/`](../LeapPort/)
via [`../LeapOS/rtems-build/`](../LeapOS/rtems-build/). This tree is gateway-only.
