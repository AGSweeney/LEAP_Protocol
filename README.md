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
docs/LEAP_PROTOCOL_SPECIFICATION.md   normative spec
docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md   CRC check values and frame vectors
schemas/leap-manifest-schema.json     JSON Schema for device/profile manifests
tools/wireshark/leap_dissector.lua    Wireshark dissector (initial)
src/                                  parser, serializer, CRC engines, transport
tests/                                conformance and regression tests
examples/                             controller and device reference code
```

## Services

| ID       | Name        |                                                     |
|----------|-------------|-----------------------------------------------------|
| `0x0001` | `LEAP-MGMT` | sessions, ownership, state machine, watchdog, fault |
| `0x0002` | `LEAP-DISC` | discovery, identity, locate-device                  |
| `0x0003` | `LEAP-DIR`  | directory, object access, profile and endpoint data |
| `0x0010` | `LEAP-PD`   | cyclic process-data read, write, exchange           |
| `0x0020` | `LEAP-DIAG` | counters, timing, event log, trace marks            |

## Development notes

Protocol changes go into the spec first, then the header, then vectors and
schema as needed. The dissector is extended as services and profiles stabilize.

LEAP v1.0 assumes an isolated machine network. It is not appropriate for
plant-wide or routed networks without an authentication extension — the owner
lease is not an access control mechanism on open networks. See spec §17.

## Current capabilities

The reference stack provides a full device path and a controller bootstrap FSM:

| Layer | Device | Controller |
| --- | --- | --- |
| DISC | HELLO reply, identity | HELLO broadcast, `leap_disc_controller` |
| DIR | profile/endpoints | SELECT_PROFILE, profile reply parsing |
| MGMT | sessions, lease, watchdog, state | open session, set OP, heartbeat, owner release |
| PD | pack/unpack, I/O shadow, sequence policy | cyclic exchange, cycle metrics |
| Stack | `leap_device_stack` (dispatch + tick) | `leap_controller_stack`, `leap_controller_session_hub` |

After bootstrap the controller stack tracks inbound peer sequence numbers (duplicate
detection, `ack_sequence` on outbound frames), dispatches async MGMT replies and
ERROR payloads to `FAULT`, and supports graceful `OWNER_RELEASE` shutdown.

Multi-device discovery uses `leap_controller_peer_table_discover()` (broadcast HELLO,
collect up to 16 peers). Known peers can be brought to OP with
`leap_controller_stack_bootstrap_peer()` without repeating discovery.

Concurrent multi-peer control uses `leap_controller_session_hub`: each bound peer
gets an independent `LeapControllerStack` (session ID, sequence, lease, PD state).
Use `leap_controller_session_hub_run_round_robin()` for cyclic I/O across all OP
peers on one shared transport.

## Roadmap

### Done (reference stack)

- Wire contract (`leap_protocol.h`), normative spec, golden frame vectors, manifest schema
- Frame parser/serializer, CRC engines, fragment handling, fuzz/regression tests
- Device-side **LEAP-MGMT** (sessions, ownership, lease/watchdog, state machine)
- **LEAP-DISC** / **LEAP-DIR** device handlers and controller helpers
- **LEAP-PD** pack/unpack, device I/O binding, controller cyclic exchange + lease maintenance
- Integrated `leap_device_stack` (DISC + DIR + MGMT + PD dispatch and tick)
- Linux `AF_PACKET` transport with partial-send retry, promisc/filter options, transport counters
- Linux loopback examples (controller + device), WSL limitation documented
- Controller stack Phase 1–3: bootstrap FSM, Linux IO adapter, `on_frame`, `release`
- Automated comms-loss unit test (`leap_device_stack_tick` lease expiry)
- CI: build, ctest, Linux example binaries, loopback wire smoke on `lo`

### Next

- Sequence/ACK window enforcement and replay hardening
- **LEAP-DIAG** service (counters, timing, event log)
- Wireshark dissector coverage for all v1.0 services and PD profiles
- Embedded reference port (no Linux sockets; timer-driven tick)
- Production EtherType registration path (beyond development `0x88B6`)
- v0.9 / v1.0 release tag once independent implementations pass conformance vectors

## License

MIT — see [LICENSE](LICENSE).
