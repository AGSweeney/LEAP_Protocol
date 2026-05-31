# `src`

Reference stack implementation — protocol handlers, integration layers, and transport adapters.

## Layout

| Path | Contents |
| --- | --- |
| `crc/` | CRC-16/XMODEM and CRC-32C engines |
| `frame/` | LEAP header parser and serializer (`leap_frame_write`) |
| `services/disc/` | LEAP-DISC device + controller handlers |
| `services/dir/` | LEAP-DIR device + controller helpers |
| `services/mgmt/` | LEAP-MGMT device, controller, and frame dispatch |
| `services/pd/` | LEAP-PD common pack/unpack, device handler, controller cyclic runner |
| `services/diag/` | LEAP-DIAG device handler + controller builders/parsers |
| `leap_device_stack.c` | Device-side DISC + DIR + MGMT + PD + DIAG dispatch + tick |
| `leap_controller_stack.c` | Controller bootstrap FSM, PD helpers, `read_diag` |
| `leap_controller_session_hub.c` | Concurrent multi-peer controller sessions |
| `leap_controller_peer.c` | Discovery peer table |
| `leap_controller_sequence.c` | Per-peer Ethernet sequence / replay protection |
| `leap_log.c` | Optional `LEAP_LOG_SECURITY` field diagnostics |
| `transport/leap_raw_linux.c` | Linux AF_PACKET raw Ethernet |
| `transport/leap_raw_winpcap.c` | Windows Npcap raw Ethernet |

## Transport notes

- **Linux:** `leap_raw_linux.c` is the primary wire transport for examples and tests.
- **Windows:** `leap_raw_winpcap.c` provides Npcap I/O for `leap_win_smoke` and `leap_win_*` examples.
- **Windows unit tests:** `leap_raw_linux.c` compiles as stubs on Windows so `leap_tests` links without AF_PACKET.

Sockets and platform I/O stay in `transport/` and `examples/` — not inside `src/services/`.

Full public API map: [docs/README.md](../docs/README.md).
