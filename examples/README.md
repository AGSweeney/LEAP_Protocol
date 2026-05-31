# `examples`

Reference implementations for controller and device bring-up.

## Planned Examples

| Example | Description |
| --- | --- |
| `device_minimal/` | Simulated MGMT session, OP transition, LEAP-PD write, spoof + tick demo |
| `linux_loopback/` | Linux raw-socket HELLO / HELLO_REPLY discovery (requires root/CAP_NET_RAW) |
| `controller_minimal/` | Discovery, session open, cyclic EXCHANGE_ENDPOINTS loop |
| `vector_replay/` | Packet replay utility driven by `LEAP_GOLDEN_FRAME_VECTORS.md` |
