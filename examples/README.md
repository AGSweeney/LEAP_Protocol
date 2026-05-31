# `examples`

Reference implementations for controller and device bring-up.

## Porting path vs learning examples

| Example | Stack usage | Role |
| --- | --- | --- |
| `linux_loopback/leap_linux_device` | `leap_device_stack` only | **Porting template** — recv loop + tick + I/O shadow |
| `linux_loopback/leap_linux_controller` | `leap_controller_stack` only | **Porting template** — bootstrap + PD |
| `linux_loopback/leap_linux_hub` | `leap_controller_session_hub` | **Multi-peer template** — discover → bootstrap_table → round-robin |
| `linux_loopback/leap_linux_discover` | peer table only | HELLO scan utility |
| `device_minimal/` | `leap_device_stack` + hand-built frames | **Learning / fuzz** — not the production porting path |

### PD / MGMT audit (May 2026)

| File | Duplicate sequence / ownership logic? | Notes |
| --- | --- | --- |
| `linux_loopback/device_main.c` | No | All protocol handling via `leap_device_stack_process_frame` + `tick` |
| `linux_loopback/controller_main.c` | No | Bootstrap, PD, DIAG via `leap_controller_stack` |
| `linux_loopback/hub_main.c` | No | Discovery table + `leap_controller_session_hub` |
| `device_minimal/main.c` | No | Uses stack for all frame processing; local `build_frame()` is transport simulation only |

## Linux loopback

See [linux_loopback/README.md](linux_loopback/README.md).

## Planned examples

| Example | Description |
| --- | --- |
| `controller_minimal/` | In-memory discovery + session open + cyclic PD (no sockets) |
| `vector_replay/` | Packet replay utility driven by `LEAP_GOLDEN_FRAME_VECTORS.md` |
