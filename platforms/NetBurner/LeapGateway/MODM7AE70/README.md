# LEAP Gateway (NetBurner MOD5441X)

**Embedded Leap-Gateway firmware for NetBurner MOD5441X modules**

| | |
|---|---|
| **Platform** | NetBurner MOD5441X ([MOD54415](https://www.netburner.com/products/system-on-modules/mod5441x/) / [MOD54417](https://www.netburner.com/products/system-on-modules/mod5441x/)) |
| **Toolchain** | NetBurner NNDK 3.5.x - NBEclipse |
| **License** | [MIT](LICENSE) |

This firmware is the **embedded Leap-Gateway** port. It links the same LEAP session hub, EIP bridge, process-data path, config parser, and gateway core used by the Alpine `leapos-gateway-alpine` image, with NetBurner-specific networking, HTTP, storage, raw Ethernet, and OpENer integration.

**MOD54417** (dual Ethernet): Port 1 = Plant / EtherNet/IP (OpENer); Port 2 = LEAP Master (raw EtherType). **MOD54415** uses a single port for both roles when only one interface is present.

> **Backport note:** NetBurner carries extra OpENer features (identity lifecycle, CM stats, settable TCP/IP, deferred reboot, etc.) documented in [`BACKPORT.md`](BACKPORT.md) for eventual merge into the Alpine i386 Linux gateway.

---

## Current scope

| Component | Status |
|-----------|--------|
| LEAP Master session hub (DISC -> DIR -> MGMT -> OP -> PD) | **Working** - shared `gateway_leap_session.c` + `leap_core` |
| Process Data exchange | **Working** - explicit little-endian wire serialization, cyclic PD, heartbeat/retry handling |
| Endianness handling | **Resolved** - NetBurner is big-endian; LEAP wire structs and PD payloads are serialized/deserialized explicitly as little-endian |
| EIP bridge assemblies 100/150/151 | **Working** - OpENer assembly hooks feed `leap_eip_bridge` directly |
| Config persist (NNDK appdata) | **Working** - mappings, cyclic period, and network settings |
| Network Web UI + API | **Working** - DHCP, static, AutoIP, dual-port modes |
| Mapping + LEAP Web UI (`/mapping.html`) | **Working** - connect, disconnect, discover, 500 ms status/I/O refresh |
| LeapOS REST aliases (`/api/v1/*`) | **Working** - GET-based aliases for NNDK HTTP callback support |
| OpENer EtherNet/IP | **Working** - enhanced port overlay (see `BACKPORT.md`) |
| Reliability pass | **Applied** - LEAP-task-owned disconnect, RX semaphore queue hardening, runtime lock around shared bridge/config access |
| Logging | **Reduced** - routine LEAP core stdout logs disabled; warnings/errors remain visible |

### Verified status

Latest local validation:

- Host build: `leap_core` and `leap_tests` build clean.
- Host tests: `145 test(s), 0 failure(s)`.
- NetBurner firmware: `Release/MOD54417_Gateway.bin` builds successfully with no warnings shown in the final build output.
- Current compressed firmware size: about `497 KB` flash, about `1.57 MB` RAM reported by the NetBurner build summary.

Known behavior and assumptions:

- LEAP wire data is little-endian. The NetBurner MOD5441X CPU is big-endian, so the port avoids native struct casting for protocol payloads and uses explicit LE reads/writes for frames, DISC/DIR/MGMT fields, and PD exchange headers/status/data.
- MOD54417 Port 1 is the Plant / EtherNet/IP side; Port 2 is the LEAP raw-EtherType side.
- MOD54415 can run single-port mode when only one interface is present.
- NetBurner HTTP routes use GET callbacks for config/control operations because the NNDK HTTP server path used here does not provide the same POST/PUT callback model as the Alpine gateway.
- The NetBurner firmware carries OpENer enhancements that should be backported to the Alpine gateway when the port is consolidated.

---

## Repository layout

```
MOD54417_Gateway/
+-- README.md                     This file
+-- BACKPORT.md                   OpENer enhancements to backport to Alpine Linux
+-- build-opener-netburner.ps1    Build libopener_netburner.a
+-- build-leap-gateway-netburner.ps1  Build libleap_gateway_netburner.a
+-- html/                         Web UI (comphtml -> firmware)
|   +-- index.html                Home
|   +-- network.html              IPv4 + dual-Ethernet
|   +-- mapping.html              LEAP mappings + session controls
|   +-- help.js
+-- src/
|   +-- main.cpp                  RTOS entry, LEAP stack init, OpENer task
|   +-- platform/                 NetBurner port (transport, net, storage, time)
|   +-- core/                     Mapping state + shared runtime helpers
|   +-- http/                     Network, mapping, and LEAP REST handlers
+-- overload/nbrtos/source/       netrx.cpp - raw EtherType RX hook for LEAP
+-- opener/                       OpENer-Enhanced NetBurner overlay
+-- leap/build-work/              libleap_gateway_netburner.a
+-- Release/                      MOD54417_Gateway.bin, .elf, .map
```

Shared sources (not duplicated in this tree):

- `platforms/x86-32/D945GSEJT/LeapGateway/src/` - `gateway_global`, `gateway_leap_session`, `gateway_pd_io`, `gateway_rtems_io`, `leap_gateway_opener`
- `leap_core/` - protocol stack, session hub, EIP bridge, config parser

---

## Requirements

| Requirement | Details |
|-------------|---------|
| NetBurner NNDK | **3.5.x** (default: `C:\nburn`) |
| IDE | **NBEclipse** (optional; CLI build works) |
| Target | **MOD5441X** - MOD54415 or MOD54417 |
| OpENer-Enhanced | `D:\OpENer-Enhanced` (override with `OPENER_ROOT`) |

---

## Build

From `MOD54417_Gateway/`:

```powershell
# After OpENer or opener/netburner_port/ changes:
.\build-opener-netburner.ps1

# After leap_core, shared LeapGateway sources, or src/platform/ changes:
.\build-leap-gateway-netburner.ps1

# Firmware link (first time or after overload/netrx change, rebuild archive):
cd Release
$env:NNDK_ROOT = "C:\nburn"
make nball    # only when overload/ changes (e.g. netrx.cpp)
make all
```

Output: `Release/MOD54417_Gateway.bin` (~497 KB compressed flash as of the latest verified build).

### Build variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `OPENER_ROOT` | `D:\OpENer-Enhanced` | OpENer-Enhanced checkout |
| `NNDK_ROOT` | `C:\nburn` | NetBurner NNDK install |
| `LEAP_OPENER_BUILD_DIR` | `%TEMP%\leap-opener-netburner` | OpENer cmake staging |
| `LEAP_GW_BUILD_DIR` | `%TEMP%\leap-gateway-netburner` | Gateway lib object dir |

---

## Runtime architecture

```
Port 1 (Plant)                    Port 2 (LEAP)
     |                                  |
     v                                  v
 OpENer EIP                     leap_transport_nb.cpp
 Assemblies 100/150/151          SetCustomNetDoRX (overload/netrx.cpp)
     |                                  |
     +---------- leap_eip_bridge -------+
                    |
           gateway_leap_session (NBRtos task)
                    |
              leap_controller_session_hub
```

At boot, `main.cpp`:

1. Brings up network (DHCP / AutoIP fallback).
2. Loads LEAP gateway config from NNDK appdata, initializes LEAP transport on Port 2, and registers the NetBurner monotonic clock for LEAP core logs.
3. Starts the LEAP session worker task and auto-connects if mappings exist.
4. Starts OpENer on Port 1 (interface `"1"` or first interface).

The LEAP session worker owns connection teardown and reconnect decisions. HTTP disconnect requests set a pending flag and are consumed by the LEAP task, which avoids concurrent release of session-hub transport state from the HTTP task. Shared bridge/config access between HTTP, OpENer, and the LEAP task is serialized by the gateway runtime lock.

---

## Commissioning

Open **`http://<gateway-ip>/`**.

| Step | Page | Actions |
|------|------|---------|
| 1 | Network (`/network.html`) | Set DHCP or static on Plant port; **MOD54417:** bridge or independent dual-port. Save; reboot if IP changed. |
| 2 | Mappings (`/mapping.html`) | Add LEAP peer MAC -> EIP byte mappings and cyclic period. Save. Use Connect / Disconnect / Discover / live I/O status. |
| 3 | EtherNet/IP | RSLinx / Studio 5000 - device `LEAP-Gateway`, assemblies 100/150/151. |

Config file format matches Alpine (`leap_gateway_config.c` key=value text). Example:

```
cyclic_ms=50
mapping.begin=0
mapping.mac=aa:bb:cc:dd:ee:ff
mapping.input.byte=0
mapping.output.byte=2
mapping.status.byte=4
```

---

## HTTP API

### Leap-Gateway aliases (`/api/v1/*`)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/status` | Runtime JSON (ipv4, leap_phase, mappings, eip_ok, ...) |
| GET | `/api/v1/leap/connect` | Request LEAP session connect |
| GET | `/api/v1/leap/disconnect` | Disconnect + suppress auto-connect |
| GET | `/api/v1/leap/discover` | 3 s LEAP scan |
| GET | `/api/v1/leap/peers` | Discovered peers |
| GET | `/api/v1/io` | Live mapping I/O |
| GET | `/api/v1/config/apply` | Persist LEAP gateway config to NNDK appdata |
| GET | `/api/v1/system/reboot` | Deferred reboot |

Legacy paths under `/api/leap/*` and `/api/config/persist` are also registered.

> Alpine uses POST/PUT for several of these; NetBurner maps them to GET because the NNDK HTTP server registers GET callbacks only.

`/api/v1/io` reports the live LEAP input/output values, E/IP input/output assembly bytes, status bytes, and peer communication state used by the mapping page's 500 ms refresh.

### NetBurner-specific

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/network/config` | Per-interface IPv4 JSON |
| GET | `/api/network/config/save` | Apply network settings (`?reboot=1` optional) |
| GET | `/api/mapping/config` | Read bridge mappings |
| GET | `/api/mapping/config/save` | Update mappings via query params |

---

## MOD54417 dual Ethernet

| Mode | Behavior |
|------|----------|
| **Bridge both ports** (default) | Single LAN, single IP |
| **Independent ports** | Ethernet0 (Plant) and Ethernet1 (LEAP) each have their own IP |

LEAP transport binds to Port 2 when present; OpENer listens on Port 1. The LEAP receive path uses the NetBurner raw Ethernet hook (`SetCustomNetDoRX`) and a semaphore-backed RX queue so the LEAP worker sleeps until a matching frame arrives instead of busy polling.

---

## OpENer (EtherNet/IP)

Build library: `.\build-opener-netburner.ps1` -> `opener/build-work/libopener_netburner.a`

Default assemblies:

| Assembly | ID | Role |
|----------|-----|------|
| Input | 100 | Produced to PLC (64 B) |
| Output | 150 | Consumed from PLC (64 B) |
| Config | 151 | Connection config (10 B) |

See [`BACKPORT.md`](BACKPORT.md) for NetBurner-only CIP enhancements to merge into `LeapGateway-linux`.

The strong OpENer hook implementations are linked from `src/main.cpp` so the runtime assembly callbacks update and read the shared LEAP/EIP bridge rather than the weak NetBurner overlay stubs.

### Run/Idle headers on MODM7AE70 (ARM little-endian)

This project disables OpENer Run/Idle headers in both directions:

- `CipRunIdleHeaderSetO2T(false);`
- `CipRunIdleHeaderSetT2O(false);`

Current implications:

- Assemblies 100/150 remain fixed payload sizes (32 B each) with no 4-byte Run/Idle prefix.
- `RunIdleChanged(EipUint32 run_idle_value)` is still used for identity state handling (`run_idle_value & 0x0001U`).
- Because MODM7AE70 is ARM little-endian, there is no extra Run/Idle byte-swap path required for this value handling.
- IPv4 endianness conversion is a separate concern and remains intentional in:
  - `opener/netburner_port/netburner_ifconfig.cpp`
  - `opener/netburner_port/nb_nvtcpip.cpp`

---

## Deploy firmware

### First load (factory MOD5441x)

1. Build Release, then `make app-s19` for `MOD54417_Gateway_APP.s19`.
2. Flash via [AutoUpdate](https://www.netburner.com/download/autoupdate-windows/) or serial boot monitor (`fla` + `.bin`).

### Updates (gateway already running)

```bat
make loadapp LOAD_TARGET=NBIMAGE NBIMAGE=MOD54417_Gateway.bin DEVIP=<device-ip>
```

---

## License

Application source in `src/` and `html/` is released under the [MIT License](LICENSE).

Copyright (c) 2026 Adam G. Sweeney

NetBurner SDK and runtime libraries remain subject to [NetBurner license terms](https://www.netburner.com).
