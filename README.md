# LEAP Protocol Repository

LEAP (Lightweight Ethernet Application Protocol) is a deterministic, ownership-based, raw Layer-2 Ethernet control protocol for embedded devices and remote I/O.

## Repository Layout

```text
.
├── docs/
│   ├── LEAP_PROTOCOL_SPECIFICATION.md
│   └── vectors/
│       └── LEAP_GOLDEN_FRAME_VECTORS.md
├── examples/
├── inc/
│   └── leap/
│       └── leap_protocol.h
├── schemas/
│   └── leap-manifest-schema.json
├── src/
├── tests/
└── tools/
    └── wireshark/
        └── leap_dissector.lua
```

## Artifacts

| File | Role |
| --- | --- |
| `inc/leap/leap_protocol.h` | Canonical wire contract: constants, packed structs, service/profile IDs, static size assertions |
| `docs/LEAP_PROTOCOL_SPECIFICATION.md` | Normative protocol specification |
| `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md` | CRC check values and golden frame byte streams for conformance testing |
| `schemas/leap-manifest-schema.json` | JSON Schema for implementation-independent LEAP device manifests |
| `tools/wireshark/leap_dissector.lua` | Wireshark Lua dissector for LEAP frame inspection (initial) |

## Development Workflow

1. Protocol rule changes go into `docs/LEAP_PROTOCOL_SPECIFICATION.md` first.
2. `inc/leap/leap_protocol.h` is kept synchronized with the specification.
3. New or updated frame vectors are added to `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md`.
4. `schemas/leap-manifest-schema.json` is updated when identity or channel metadata contracts change.
5. `tools/wireshark/leap_dissector.lua` is extended as services and profiles stabilize.

## Implementation Areas

| Directory | Contents |
| --- | --- |
| `src/` | Protocol parser, serializer, CRC engines, transport glue |
| `tests/` | Vector-driven parser/CRC checks, conformance tests |
| `examples/` | Controller and device reference implementations |
