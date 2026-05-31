# LEAP Protocol

**Lightweight Ethernet Application Protocol** — a deterministic, raw Layer 2
Ethernet control protocol for embedded devices and remote I/O on isolated
industrial machine networks.

Status: **Draft v1.0** — wire contract and specification are stable for
independent implementation and conformance testing. Not yet tagged for
production release.

---

## What LEAP Is

LEAP runs directly on Ethernet without IP addressing. Controllers discover and
manage devices by MAC address. A single controller may hold an exclusive owner
lease on a device at a time; outputs are applied only while the lease is active
and the device is in the `OP` state. On lease expiry, watchdog timeout, stale
frames, or communication loss, the device autonomously applies its configured
safe outputs without waiting for a controller command.

LEAP is designed for private machine-cell networks using standard commodity
Ethernet switches. It does not require managed switches, VLANs, or
infrastructure beyond normal Gigabit or Fast Ethernet hardware.

---

## Design Principles

1. No IP configuration required — MAC-address-only discovery and ownership.
2. One owner at a time — exactly one controller holds the active control lease.
3. Standard switches supported — no proprietary switch firmware needed.
4. Deterministic process data — explicitly sequenced, CRC-protected cyclic I/O.
5. Autonomous safe behavior — outputs go safe on any communication loss without
   a controller stop command.
6. Profile-driven — I/O layout and semantics are defined by profiles, not
   hardcoded into the base protocol.
7. Human-friendly commissioning — mandatory identity and locate-device support.

---

## Services

| Service ID | Name        | Purpose                                               |
| ---------: | ----------- | ----------------------------------------------------- |
| `0x0001`   | `LEAP-MGMT` | Sessions, ownership, state machine, watchdog, fault   |
| `0x0002`   | `LEAP-DISC` | Discovery, identity, locate-device commissioning      |
| `0x0003`   | `LEAP-DIR`  | Directory, object access, profile and endpoint tables |
| `0x0010`   | `LEAP-PD`   | Cyclic process-data read, write, and exchange         |
| `0x0020`   | `LEAP-DIAG` | Counters, timing, event log, trace marks              |
| `0x8000..` | Vendor      | Private service extensions                           |

---

## Repository Layout

```text
.
├── docs/
│   ├── LEAP_PROTOCOL_SPECIFICATION.md   ← normative spec
│   └── vectors/
│       └── LEAP_GOLDEN_FRAME_VECTORS.md ← CRC and frame conformance vectors
├── examples/                            ← controller and device references
├── inc/
│   └── leap/
│       └── leap_protocol.h              ← canonical wire contract
├── schemas/
│   └── leap-manifest-schema.json        ← device/profile manifest schema
├── src/                                 ← protocol implementation modules
├── tests/                               ← conformance and regression tests
└── tools/
    └── wireshark/
        └── leap_dissector.lua           ← Wireshark dissector (initial)
```

---

## Key Artifacts

| File | Role |
| ---- | ---- |
| `inc/leap/leap_protocol.h` | Canonical wire contract: packed structs, constants, service and profile IDs, compile-time size assertions |
| `docs/LEAP_PROTOCOL_SPECIFICATION.md` | Normative protocol specification covering all services, state machine, watchdog rules, fragmentation, and conformance targets |
| `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md` | CRC check values and golden frame byte streams for automated parser and conformance verification |
| `schemas/leap-manifest-schema.json` | JSON Schema 2020-12 for implementation-independent device and profile manifests |
| `tools/wireshark/leap_dissector.lua` | Wireshark Lua dissector for live frame inspection during development |

---

## Development Workflow

1. Protocol rule changes go into `docs/LEAP_PROTOCOL_SPECIFICATION.md` first.
2. `inc/leap/leap_protocol.h` is kept synchronized with the specification.
3. Frame vectors in `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md` are updated
   whenever header layout, CRC coverage, or payload structures change.
4. `schemas/leap-manifest-schema.json` is updated when identity or channel
   metadata contracts change.
5. `tools/wireshark/leap_dissector.lua` is extended as services and profiles
   stabilize.

---

## Implementation Areas

| Directory  | Contents                                                  |
| ---------- | --------------------------------------------------------- |
| `src/`     | Parser, serializer, CRC engines, transport glue           |
| `tests/`   | Vector-driven parser/CRC checks, conformance tests        |
| `examples/`| Controller and device reference implementations           |

---

## v1.0 Pre-Flight Checklist

Before tagging the v1.0.0 release, the following must be complete:

- [ ] All `LEAP_STATIC_ASSERT` layout checks pass on GCC, Clang, and MSVC
- [ ] Automated tests pass against all vectors in `LEAP_GOLDEN_FRAME_VECTORS.md`
- [ ] Minimum 50-byte Ethernet payload padding verified on target drivers
- [ ] Safety watchdog transition tested under switch congestion conditions
- [ ] Production EtherType registration plan confirmed
- [ ] Vector 3 (`HELLO_REPLY`) flags corrected and `header_crc16` recomputed
- [ ] `leap-manifest-schema.json` `schema_version` field set to `"1.0"`

See `docs/LEAP_PROTOCOL_SPECIFICATION.md §22.1` for the full checklist.

---

## Security Scope

LEAP v1.0 is designed for **isolated private machine-cell networks only**.
The owner lease mechanism is not a substitute for access control on networks
where unauthenticated nodes can join. LEAP MUST NOT be used on plant-wide,
enterprise IT/OT, or internet-routed networks without an authentication and
authorization extension. See specification §17 for the full security model and
deployment boundary.

---

## License

MIT — see [LICENSE](LICENSE).
