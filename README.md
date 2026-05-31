# LEAP Protocol

LEAP (Lightweight Ethernet Application Protocol) is a raw Layer 2 Ethernet
control protocol for remote I/O and embedded devices on isolated machine
networks. It runs without IP addressing — controllers find and own devices by
MAC address, negotiate a process-data profile, then exchange cyclic I/O. On
owner lease expiry, watchdog timeout, or any communication loss, the device
applies its configured safe outputs without needing a controller stop command.

Status: draft v1.0. Wire contract and spec are stable enough for independent
implementation and conformance testing. Not tagged for production release yet.

LEAP targets private machine-cell networks on standard Ethernet switches. No
managed switch firmware, VLANs, or special infrastructure required.

## Repository

```
inc/leap/leap_protocol.h              wire contract — packed structs, constants,
                                      service/profile IDs, static size checks
docs/                                 spec, stack guides, golden vectors (see docs/README.md)
schemas/leap-manifest-schema.json     JSON Schema for device/profile manifests
tools/wireshark/leap_dissector.lua    Wireshark dissector (v1 services + PD exchange)
tools/ci/wire_smoke_*                 manual end-to-end tests (Linux shell, Windows PS1)
src/                                  reference stack (see src/README.md)
tests/                                conformance and regression tests (111–112)
examples/linux_loopback/              Linux AF_PACKET device, controller, hub, discover
examples/win_l2/                        Windows Npcap device, controller, hub, discover
examples/win_smoke/                     Windows Npcap single-process wire smoke
examples/device_minimal/              learning / fuzz harness (not porting template)
platforms/clearcore/                  ClearCore LEAP device firmware (ProjectTemplate)
```

## Services

| ID | Name | Reference stack |
| ---: | --- | --- |
| `0x0001` | `LEAP-MGMT` | Sessions, ownership, state machine, watchdog, fault |
| `0x0002` | `LEAP-DISC` | Discovery, identity, locate-device |
| `0x0003` | `LEAP-DIR` | Directory, profile/endpoints, object read |
| `0x0010` | `LEAP-PD` | Cyclic read, write, exchange |
| `0x0020` | `LEAP-DIAG` | Counters, timing, event log, trace marks |

## Development notes

Protocol changes go into the spec first, then the header, then vectors and
schema as needed. The dissector is extended as services and profiles stabilize.

LEAP v1.0 assumes an isolated machine network. It is not appropriate for
plant-wide or routed networks without an authentication extension — the owner
lease is not an access control mechanism on open networks. See spec §17.

## Reference stack overview

### Device path

**`leap_device_stack`** — single entry point for inbound frames and periodic tick:

- Dispatches DISC, DIR, MGMT, PD, DIAG
- Records DIAG counters/events on parse errors, PD rejects, MGMT transitions
- Calls `leap_mgmt_process_tick()` for lease/watchdog expiry

### Controller path

**`leap_controller_stack`** — bootstrap FSM and PD helpers:

- `bootstrap()` / `bootstrap_peer()` → OP
- `on_frame()` for async MGMT; frame sequence + session binding
- `run_cyclic_pd()` / `pd_single_write()` after OP
- `read_diag()` / `log_diag()` — post-OP DIAG counters and timing
- `release()` — graceful OWNER_RELEASE

**`leap_controller_session_hub`** — N concurrent device sessions (independent
session ID, MGMT sequence, lease, PD state, frame sequence per slot).

### Service modules (`src/services/`)

| Module | Device | Controller |
| --- | --- | --- |
| DISC | `leap_disc_device` | `leap_disc_controller` |
| DIR | `leap_dir_device` | `leap_dir_controller` |
| MGMT | `leap_mgmt_device`, `leap_mgmt_process` | `leap_mgmt_controller` |
| PD | `leap_pd_device`, `leap_pd_common` | `leap_pd_controller` |
| DIAG | `leap_diag_device` | `leap_diag_controller` |

PD controller stats include cycle **latency**, **jitter** vs target period,
**lost frames** (exchange timeouts), reply rejects, and overruns — see
`leap_pd_controller_log_stats()`.

### Multi-peer hardening

Per-peer Ethernet sequence tracking, optional gap/out-of-window rejection,
session binding after OP, PD exchange validation (profile + process_sequence +
§13.4 frame age), foreign-owner skip on hub bootstrap, discovery early exit at
`min_peers`, Linux recv demux by peer MAC, timestamped logging via `leap_log.h`,
optional `LEAP_LOG_SECURITY` stderr diagnostics.

Details: [docs/LEAP_MULTI_PEER_NOTES.md](docs/LEAP_MULTI_PEER_NOTES.md)

### Porting gate

Before porting to embedded or Windows masters:

1. Device app uses **`leap_device_stack`** only.
2. Controller app uses **`leap_controller_stack`** or **session hub** — no duplicated MGMT/PD logic.
3. Transport stays outside `src/services/` (callbacks / your link layer).
4. Run **`ctest`** green on the host toolchain before diverging.

Full module map: [docs/README.md](docs/README.md)

## Quick start (Linux example)

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure

# Terminal 1
sudo ./build/leap_linux_device lo

# Terminal 2
sudo ./build/leap_linux_controller lo
# or: sudo ./build/leap_linux_controller --cyclic lo
```

See [examples/linux_loopback/README.md](examples/linux_loopback/README.md).

### Windows examples (Npcap)

Build with `-DLEAP_BUILD_WIN_SMOKE=ON` and/or `-DLEAP_BUILD_WIN_L2=ON`:

```powershell
cmake -S . -B build-win -DLEAP_BUILD_WIN_SMOKE=ON -DLEAP_BUILD_WIN_L2=ON
cmake --build build-win --config Release
.\build-win\Release\leap_win_smoke.exe
```

See [examples/win_smoke/README.md](examples/win_smoke/README.md) and
[examples/win_l2/README.md](examples/win_l2/README.md).

### ClearCore LEAP device (ProjectTemplate)

Pair a ClearCore on the wire with `leap_win_controller` or `leap_linux_controller`:

```powershell
powershell -ExecutionPolicy Bypass -File platforms/clearcore/import_project_template.ps1
powershell -ExecutionPolicy Bypass -File platforms/clearcore/open_studio.ps1
# Open: platforms/clearcore/LeapDeviceFirmware/LeapDeviceFirmware.atsln
# Build ClearCore -> LwIP -> LeapDeviceFirmware (Release)
```

See [platforms/clearcore/README.md](platforms/clearcore/README.md).

## Roadmap

### Done (reference stack)

- Wire contract, normative spec, golden vectors (incl. DIAG §10), manifest schema
- Frame parser/serializer, CRC, fragment rejection policy, fuzz/regression tests (111–112)
- All five v1 services: device handlers + controller helpers (where applicable)
- **`leap_device_stack`** — DISC + DIR + MGMT + PD + DIAG + tick
- **`leap_controller_stack`** — bootstrap, `on_frame`, `release`, `run_cyclic_pd`, `read_diag`
- **`leap_controller_session_hub`** + peer discovery table + `leap_linux_hub` example
- Multi-peer hardening (sequence, session bind, PD validation, frame age, security log)
- Linux AF_PACKET transport + Windows Npcap transport (`leap_raw_winpcap`)
- Linux loopback/hub/discover examples; Windows `leap_win_smoke` + `leap_win_device`/`leap_win_controller`/`leap_win_hub`/`leap_win_discover`
- Timestamped example logging (`leap_log_printf`) across Linux and Windows examples
- ClearCore device firmware port (`platforms/clearcore`)
- Wireshark dissector: v1 services, PD exchange, DIAG message types
- CI: Linux build + unit tests (111) + example binary checks; Windows build + unit tests (112)

### Next (see [docs/LEAP_FORWARD_PLAN.md](docs/LEAP_FORWARD_PLAN.md))

**Near term (core lock-down)**

- ~~PD/example audit~~ **done** — porting path is `linux_loopback/*` and `win_l2/*`; `device_minimal` stays learning-only
- ~~Controller DIAG read helper + `--diag` flag~~ **done** (Linux + Windows controllers)
- ~~Hub integration tests (round-robin, foreign-owner skip)~~ **done**
- Manual wire smoke on native Linux (`tools/ci/wire_smoke_*.sh`) and Windows (`wire_smoke_win.ps1`) before platform ports

**Medium term (3–4 weeks)**

- ~~Transport link monitoring and reconnect policy~~ **done** — see [docs/LEAP_TRANSPORT_RECONNECT.md](docs/LEAP_TRANSPORT_RECONNECT.md)
- ~~DIAG golden vectors + Wireshark coverage~~ **done**
- ~~Multi-device hub example~~ **done** — `leap_linux_hub`; hub `--diag` variant still open
- Expand transport stats in periodic example logs (drops, retries, parse errors)
- v1.0 conformance / release readiness review

**Later**

- Rolling 64-bit sequence bitmap, fragment reassembly, multi-controller election
- Production EtherType assignment, release tag

## Documentation

| Doc | Description |
| --- | --- |
| [docs/README.md](docs/README.md) | Documentation index + module map |
| [docs/LEAP_FORWARD_PLAN.md](docs/LEAP_FORWARD_PLAN.md) | Prioritized 7–10 day and 3–4 week plan vs current status |
| [docs/LEAP_PROTOCOL_SPECIFICATION.md](docs/LEAP_PROTOCOL_SPECIFICATION.md) | Normative spec |
| [docs/LEAP_CONTROLLER_STACK_PLAN.md](docs/LEAP_CONTROLLER_STACK_PLAN.md) | Controller stack design + status |
| [docs/LEAP_MULTI_PEER_NOTES.md](docs/LEAP_MULTI_PEER_NOTES.md) | Multi-device / multi-controller notes |
| [docs/LEAP_TRANSPORT_RECONNECT.md](docs/LEAP_TRANSPORT_RECONNECT.md) | Link monitoring and reconnect policy |
| [examples/README.md](examples/README.md) | Example index and porting path |
| [tests/README.md](tests/README.md) | Unit test suite index (111–112 tests) |
| [platforms/clearcore/README.md](platforms/clearcore/README.md) | ClearCore firmware setup and build |

## License

MIT — see [LICENSE](LICENSE).
