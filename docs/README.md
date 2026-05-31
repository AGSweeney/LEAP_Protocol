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
| [LEAP_CONTROLLER_STACK_PLAN.md](LEAP_CONTROLLER_STACK_PLAN.md) | Controller bootstrap FSM, session hub, implementation status |
| [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) | Multi-device / multi-controller behavior, config knobs, failure modes |
| [../examples/linux_loopback/README.md](../examples/linux_loopback/README.md) | Native Linux wire example (transport + stacks) |

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
  leap_log.h                       Optional LEAP_LOG_SECURITY field diagnostics

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
  transport/leap_raw_linux.c       Linux AF_PACKET (examples + tests)
```

## Service coverage (reference stack)

| Service | Device handler | Controller helpers | In device stack | In controller stack |
| --- | --- | --- | --- | --- |
| DISC | yes | yes | yes | bootstrap |
| DIR | yes | yes | yes | bootstrap |
| MGMT | yes | yes | yes | bootstrap + `on_frame` |
| PD | yes | yes (cyclic engine) | yes | `run_cyclic_pd`, single write |
| DIAG | yes | yes (builders/parsers) | yes | not wired to FSM yet |

## Testing and CI

- **Unit tests:** `cmake --build build && ctest --test-dir build` (99 tests at last count)
- **CI (GitHub Actions):** configure, build, `ctest`, verify Linux example binaries
- **Wire smoke:** `tools/ci/wire_smoke_*.sh` — run manually on native Linux with
  `sudo`; not executed on GitHub-hosted runners (network namespace limits)

## Porting gate (before hardware targets)

1. Device application uses **`leap_device_stack`** only (`process_frame` + `tick`).
2. Controller application uses **`leap_controller_stack`** or **`leap_controller_session_hub`** — no duplicated MGMT/PD state machines.
3. Transport is behind **`LeapControllerStackIo`** / raw socket adapter; no sockets inside `src/services/`.
4. Profile and endpoint IDs come from DIR / `LeapPdProfileMap`, not hard-coded in platform code.

## Change process

Protocol changes: **spec → `leap_protocol.h` → vectors/schema → handlers → tests**.
