# LEAP Protocol

LEAP (Lightweight Ethernet Application Protocol) is a raw Layer 2 Ethernet control
protocol for remote I/O and embedded devices on isolated machine networks. It runs
without IP addressing — controllers find and own devices by MAC address, negotiate a
process-data profile, then exchange cyclic I/O. On owner lease expiry, watchdog
timeout, or communication loss, the device applies its configured safe outputs
without needing a controller stop command.

**Status:** draft v1.0. The wire contract and normative spec are stable enough for
independent implementation and conformance testing. Not tagged for production release
yet.

**June 2026 update:** NetBurner `MOD54415LC` now passes full `device_conformance`
in LEAP Conformance Studio (including cyclic WRITE and cyclic EXCHANGE).

LEAP targets private machine-cell networks on standard Ethernet switches. No managed
switch firmware, VLANs, or special infrastructure required.

---

## Repository layout

```
leap_core/                          Reference stack (C library)
  inc/leap/leap_protocol.h          Wire contract — packed structs, IDs, size checks
  inc/leap/                         Public API (stacks, transport, conformance)
  src/                              Service handlers, integration, transport
leap_cli/                           Windows host tools (Npcap)
  win_l2/                           Device, controller, hub, discover, identify
  win_smoke/                        Single-process wire smoke
  conformance/                      Conformance CLI (`leap_conformance`)
leap_studio_qt/                     LEAP Conformance Studio (Qt 6, Windows)
examples/
  linux_loopback/                   Linux AF_PACKET porting templates
  device_minimal/                   Learning / fuzz harness (not porting template)
platforms/                          Embedded device ports (see below)
docs/                               Spec, stack guides, golden vectors
schemas/leap-manifest-schema.json   JSON Schema for device/profile manifests
tests/                              Unit tests (115 Linux / 116 Windows)
tools/
  wireshark/leap_dissector.lua        Wireshark dissector (v1 services + PD + DIAG)
  ci/wire_smoke_*                     Manual end-to-end wire tests
build.ps1                           Windows MSVC build helper → local `build-win/`
docs/BUILD.md                       CMake build trees, clean workspace, CI paths
```

Local build output (`build/`, `build-win/`, …) is **gitignored** — never commit it.

---

## Services

| ID | Name | Reference stack |
| ---: | --- | --- |
| `0x0001` | `LEAP-MGMT` | Sessions, ownership, state machine, watchdog, fault |
| `0x0002` | `LEAP-DISC` | Discovery, identity, locate-device |
| `0x0003` | `LEAP-DIR` | Directory, profile/endpoints, object read |
| `0x0010` | `LEAP-PD` | Cyclic read, write, exchange |
| `0x0020` | `LEAP-DIAG` | Counters, timing, event log, trace marks |

---

## Reference stack

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
session ID, MGMT sequence, lease, PD state, and frame sequence per slot).

### Service modules (`leap_core/src/services/`)

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

### Conformance engine

**`leap_core/src/conformance/`** — scenario-driven commissioning and conformance
tests used by **`leap_conformance`** (CLI) and **LEAP Conformance Studio** (Qt).

### Multi-peer hardening

Per-peer Ethernet sequence tracking, optional gap/out-of-window rejection, session
binding after OP, PD exchange validation (profile + process_sequence + §13.4 frame
age), foreign-owner skip on hub bootstrap, Linux recv demux by peer MAC, timestamped
logging via `leap_log.h`, optional `LEAP_LOG_SECURITY` stderr diagnostics.

Details: [docs/LEAP_MULTI_PEER_NOTES.md](docs/LEAP_MULTI_PEER_NOTES.md)

### Porting gate

Before porting to embedded or Windows masters:

1. Device app uses **`leap_device_stack`** only.
2. Controller app uses **`leap_controller_stack`** or **session hub** — no duplicated MGMT/PD logic.
3. Transport stays outside `leap_core/src/services/` (callbacks / your link layer).
4. Run **`ctest`** green on the host toolchain before diverging.

Full module map: [docs/README.md](docs/README.md) · stack source layout: [leap_core/src/README.md](leap_core/src/README.md)

---

## Quick start

See [docs/BUILD.md](docs/BUILD.md) for full CMake options and clean commands.

### Linux (native)

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure

# Terminal 1
sudo ./build/leap_linux_device lo

# Terminal 2
sudo ./build/leap_linux_controller lo
```

From **Windows**, run Linux unit tests in WSL:

```powershell
.\build-wsl.ps1 -Test
```

Wire examples need **native Linux** (not WSL2 `AF_PACKET`). See
[examples/linux_loopback/README.md](examples/linux_loopback/README.md).

### Windows (Npcap + Studio)

```powershell
.\build.ps1              # Studio, conformance CLI, tests, win_l2 tools → build-win\
.\build.ps1 -Test        # build + run leap_tests
.\build.ps1 -Clean       # delete build-win and reconfigure
```

| Artifact | Path |
| --- | --- |
| Unit tests | `build-win\Release\leap_tests.exe` |
| Conformance CLI | `build-win\leap_cli\Release\leap_conformance.exe` |
| Conformance Studio | `build-win\leap_studio_qt\Release\leap_studio_qt.exe` |
| Npcap controller | `build-win\Release\leap_win_controller.exe` |

Manual CMake, Qt paths, and Npcap notes: [docs/BUILD.md](docs/BUILD.md),
[leap_studio_qt/README.md](leap_studio_qt/README.md),
[leap_cli/win_l2/README.md](leap_cli/win_l2/README.md).

Run Npcap tools as **Administrator** on physical adapters.

---

## Platform ports

Embedded firmware builds **inside each platform project** — not in the root CMake
trees. Pair devices on the wire with `leap_linux_controller` or `leap_win_controller`.

| Platform | Targets | Doc |
| --- | --- | --- |
| ClearCore | Teknic ClearCore + lwIP raw hook | [platforms/clearcore/README.md](platforms/clearcore/README.md) |
| Texas Instruments | BeagleBone (AM335x), LP-AM243 | [platforms/TI/README.md](platforms/TI/README.md) |
| Espressif | GL-C-618WL, KC868-A16 (ESP-IDF) | [platforms/Espressif/README.md](platforms/Espressif/README.md) |
| NetBurner | MOD54415LC (active port) | [platforms/NetBurner/README.md](platforms/NetBurner/README.md) |
| x86-32 / RTEMS | D945GSEJT — LeapOS bootable device (active port) | [platforms/x86-32/README.md](platforms/x86-32/README.md) |

### ClearCore quick start

```powershell
powershell -ExecutionPolicy Bypass -File platforms/clearcore/import_project_template.ps1
powershell -ExecutionPolicy Bypass -File platforms/clearcore/open_studio.ps1
# Open: platforms/clearcore/LeapDeviceFirmware/LeapDeviceFirmware.atsln
# Build ClearCore → LwIP → LeapDeviceFirmware (Release)
```

---

## Examples

Production-style examples use the stacks only — no hand-rolled MGMT/PD state machines.

| Example | Stack | Role |
| --- | --- | --- |
| `examples/linux_loopback/*` | device / controller / hub | **Linux porting templates** |
| `leap_cli/win_l2/*` | device / controller / hub | **Windows Npcap templates** |
| `leap_cli/win_smoke/*` | cooperative single-handle | **Windows wire validation** |
| `examples/device_minimal/` | stack + hand-built frames | **Learning / fuzz** — not the porting path |

Hub binaries support round-robin, parallel, and Windows **`--random-peer`** soak mode.
See [examples/README.md](examples/README.md).

---

## Development notes

Protocol changes go into the spec first, then `leap_protocol.h`, then vectors and
schema as needed. The dissector is extended as services and profiles stabilize.

LEAP v1.0 assumes an isolated machine network. It is not appropriate for plant-wide
or routed networks without an authentication extension — the owner lease is not an
access control mechanism on open networks. See spec §17.

---

## Roadmap

Shipped reference-stack capabilities are summarized above and in
[docs/LEAP_FORWARD_PLAN.md](docs/LEAP_FORWARD_PLAN.md). Remaining work includes
manual wire smoke on native hosts, hub DIAG variants, auto-reconnect FSM, and v1.0
release readiness review.

---

## Documentation

| Doc | Description |
| --- | --- |
| [docs/README.md](docs/README.md) | Documentation index + module map |
| [docs/BUILD.md](docs/BUILD.md) | CMake build trees and CI |
| [docs/LEAP_PROTOCOL_SPECIFICATION.md](docs/LEAP_PROTOCOL_SPECIFICATION.md) | Normative spec |
| [docs/LEAP_FORWARD_PLAN.md](docs/LEAP_FORWARD_PLAN.md) | Implemented capabilities and open work |
| [docs/LEAP_CONTROLLER_STACK_PLAN.md](docs/LEAP_CONTROLLER_STACK_PLAN.md) | Controller stack design + status |
| [docs/LEAP_MULTI_PEER_NOTES.md](docs/LEAP_MULTI_PEER_NOTES.md) | Multi-device / multi-controller notes |
| [docs/LEAP_TRANSPORT_RECONNECT.md](docs/LEAP_TRANSPORT_RECONNECT.md) | Link monitoring and reconnect policy |
| [examples/README.md](examples/README.md) | Example index and porting path |
| [tests/README.md](tests/README.md) | Unit test suite index (115–116 tests) |
| [leap_studio_qt/README.md](leap_studio_qt/README.md) | Conformance Studio build and usage |
| [leap_cli/win_l2/README.md](leap_cli/win_l2/README.md) | Windows Npcap examples |
| [platforms/clearcore/README.md](platforms/clearcore/README.md) | ClearCore firmware setup |

---

## License

MIT — see [LICENSE](LICENSE).
