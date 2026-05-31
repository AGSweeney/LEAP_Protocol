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

## License

MIT — see [LICENSE](LICENSE).
