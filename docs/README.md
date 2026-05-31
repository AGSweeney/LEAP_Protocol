# LEAP Protocol — documentation index

Reference documentation for the LEAP v1.0 wire protocol and the C reference stack
in this repository.

## Normative and contract

| Document | Purpose |
| --- | --- |
| [LEAP_PROTOCOL_SPECIFICATION.md](LEAP_PROTOCOL_SPECIFICATION.md) | Normative protocol (services, state machine, PD rules, diagnostics) |
| [../inc/leap/leap_protocol.h](../inc/leap/leap_protocol.h) | Packed wire structs, IDs, static size checks |
| [vectors/LEAP_GOLDEN_FRAME_VECTORS.md](vectors/LEAP_GOLDEN_FRAME_VECTORS.md) | Golden frames and CRC check values |
| [../schemas/leap-manifest-schema.json](../schemas/leap-manifest-schema.json) | Device/profile manifest JSON Schema |

## Reference stack guides

| Document | Purpose |
| --- | --- |
| [LEAP_FORWARD_PLAN.md](LEAP_FORWARD_PLAN.md) | Prioritized roadmap (7–10 days + 3–4 weeks) vs implementation status |
| [LEAP_CONTROLLER_STACK_PLAN.md](LEAP_CONTROLLER_STACK_PLAN.md) | Controller bootstrap FSM, session hub, implementation status |
| [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) | Multi-device / multi-controller behavior, config knobs, failure modes |
| [LEAP_TRANSPORT_RECONNECT.md](LEAP_TRANSPORT_RECONNECT.md) | Link monitoring (`leap_raw_linux`) and reconnect policy |
| [../examples/linux_loopback/README.md](../examples/linux_loopback/README.md) | Native Linux wire example (transport + stacks) |
| [../examples/win_smoke/README.md](../examples/win_smoke/README.md) | Windows Npcap single-process wire smoke |
| [../examples/win_l2/README.md](../examples/win_l2/README.md) | Windows Npcap two-process L2 pair |
| [../platforms/clearcore/README.md](../platforms/clearcore/README.md) | ClearCore embedded device firmware |

## Reference stack module map

```
inc/leap/                          Public API
  leap_protocol.h                  Wire contract
  leap_frame.h                     Parse / serialize
  leap_device_stack.h              Device dispatch (DISC, DIR, MGMT, PD, DIAG)
  leap_controller_stack.h          Controller bootstrap FSM + PD helpers
  leap_controller_session_hub.h    Concurrent multi-peer sessions
  leap_controller_peer.h           Discovery peer table
  leap_controller_sequence.h       Per-peer Ethernet sequence / replay
  leap_log.h                       Timestamped logging + optional LEAP_LOG_SECURITY
  leap_win_time.h                  Windows monotonic clock (Win32 builds)
  leap_raw_linux.h                 Linux AF_PACKET transport
  leap_raw_winpcap.h               Windows Npcap transport

src/services/
  disc/   leap_disc_device.c, leap_disc_controller.c
  dir/    leap_dir_device.c, leap_dir_controller.c
  mgmt/   leap_mgmt_device.c, leap_mgmt_controller.c, leap_mgmt_process.c
  pd/     leap_pd_common.c, leap_pd_device.c, leap_pd_controller.c
  diag/   leap_diag_device.c, leap_diag_controller.c

src/
  leap_device_stack.c              Device-side integration
  leap_controller_stack.c          Controller-side integration
  leap_controller_session_hub.c
  leap_controller_peer.c
  leap_controller_sequence.c
  leap_log.c
  platform/leap_win_time.c         Windows monotonic time (Win32 only)
  transport/leap_raw_linux.c       Linux AF_PACKET (examples + tests)
  transport/leap_raw_winpcap.c     Windows Npcap (examples; optional in CI)
```

Link monitoring: `leap_raw_linux_query_link`, `leap_raw_linux_poll_link` — see
[LEAP_TRANSPORT_RECONNECT.md](LEAP_TRANSPORT_RECONNECT.md). Windows transport
stats are available via `leap_raw_winpcap_get_stats()`.

## Service coverage (reference stack)

| Service | Device handler | Controller helpers | In device stack | In controller stack |
| --- | --- | --- | --- | --- |
| DISC | yes | yes | yes | bootstrap |
| DIR | yes | yes | yes | bootstrap |
| MGMT | yes | yes | yes | bootstrap + `on_frame` |
| PD | yes | yes (cyclic engine) | yes | `run_cyclic_pd`, single write |
| DIAG | yes | yes (builders/parsers + `read_diag`) | yes | `read_diag`, `--diag` (Linux + Windows controller) |

## Testing and CI

- **Unit tests:** `cmake --build build && ctest --test-dir build` — **116 tests on Windows**, **115 on Linux**
- **CI (GitHub Actions):**
  - **Linux:** configure, build, `ctest`, verify Linux example binaries exist
  - **Windows:** configure, build, `ctest` (core + tests only; Npcap examples not built in CI)
- **Wire smoke (manual):**
  - Linux: `tools/ci/wire_smoke_*.sh` — native Linux with `sudo`
  - Windows: `tools/ci/wire_smoke_win.ps1` — requires `-DLEAP_BUILD_WIN_SMOKE=ON`
  - Not executed on GitHub-hosted runners (network namespace / Npcap limits)

## Porting gate (before hardware targets)

1. Device application uses **`leap_device_stack`** only (`process_frame` + `tick`).
2. Controller application uses **`leap_controller_stack`** or **`leap_controller_session_hub`** — no duplicated MGMT/PD state machines.
3. Transport is behind **`LeapControllerStackIo`** / raw socket adapter; no sockets inside `src/services/`.
4. Profile and endpoint IDs come from DIR / `LeapPdProfileMap`, not hard-coded in platform code.

### Ready for hardware (checklist)

Sign off before starting an embedded or alternate-OS port:

| Check | How to verify |
| --- | --- |
| Unit tests green | `ctest --test-dir build --output-on-failure` (116 on Windows, 115 on Linux) |
| Linux wire path (manual) | `sudo ./build/leap_linux_device lo` + `sudo ./build/leap_linux_controller lo` on native Linux |
| Stack-only examples | `leap_linux_device` / `leap_linux_controller` use `leap_*_stack` — no hand-rolled MGMT/PD FSM in `device_main.c` |
| Multi-peer behavior | Hub tests cover round-robin, parallel, random-peer lap, foreign-owner skip; see [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) |
| Windows multi-peer | `leap_win_hub` + `leap_win_discover` mirror Linux hub/discover (build with `-DLEAP_BUILD_WIN_L2=ON`) |
| DIAG round-trip | `sudo ./build/leap_linux_controller --diag lo` with device running |
| Transport boundary | Sockets only in `examples/` and `src/transport/` — not in `src/services/` |

`examples/device_minimal` is intentionally low-level (learning / fuzz harness) and is **not** the porting template.

## Change process

Protocol changes: **spec → `leap_protocol.h` → vectors/schema → handlers → tests**.
