# LEAP Protocol Repository

`LEAP` (Lightweight Ethernet Application Protocol) is a deterministic, ownership-based, raw Layer-2 Ethernet control protocol for embedded devices and remote I/O.

This repository is organized as a production-oriented protocol workspace with clear separation between wire contracts, specification artifacts, schemas, tooling, and future implementation code.

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

## Primary Artifacts

- `inc/leap/leap_protocol.h`
  - Canonical LEAP wire contract: constants, packed structs, service/profile IDs, and static size checks.
- `docs/LEAP_PROTOCOL_SPECIFICATION.md`
  - Normative protocol specification.
- `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md`
  - CRC checks and golden frame vectors for conformance testing.
- `schemas/leap-manifest-schema.json`
  - Implementation-independent JSON schema for LEAP manifests.
- `tools/wireshark/leap_dissector.lua`
  - Initial Wireshark dissector for LEAP frame inspection.

## Intended Workflow

1. Update protocol rules in `docs/LEAP_PROTOCOL_SPECIFICATION.md`.
2. Keep `inc/leap/leap_protocol.h` synchronized with the specification.
3. Add/update vectors in `docs/vectors/LEAP_GOLDEN_FRAME_VECTORS.md`.
4. Update `schemas/leap-manifest-schema.json` when metadata expectations change.
5. Extend `tools/wireshark/leap_dissector.lua` as additional services/profiles are stabilized.

## Next Implementation Areas

- `src/`: protocol parser, serializer, CRC helpers, and transport glue.
- `tests/`: vector-driven parser/CRC/conformance tests.
- `examples/`: controller/device reference examples.
