# `examples`

Reference implementations for controller and device bring-up.

## Porting path vs learning examples

| Example | Stack usage | Role |
| --- | --- | --- |
| `linux_loopback/leap_linux_device` | `leap_device_stack` only | **Porting template** — recv loop + tick + I/O shadow |
| `linux_loopback/leap_linux_controller` | `leap_controller_stack` only | **Porting template** — bootstrap + PD + `--diag` |
| `linux_loopback/leap_linux_hub` | `leap_controller_session_hub` | **Multi-peer template** — discover → bootstrap_table → round-robin |
| `win_l2/leap_win_controller` | `leap_controller_stack` | **Windows Npcap** — bootstrap + PD + `--diag` (pair with `leap_win_device` on LAN) |
| `win_l2/leap_win_device` | `leap_device_stack` | **Windows Npcap** — recv loop + tick + I/O shadow |
| `win_smoke/leap_win_smoke` | cooperative single-handle | **Windows validation** — in-process device + relay on Npcap loopback |
| `linux_loopback/leap_linux_discover` | peer table only | HELLO scan utility |
| `device_minimal/` | `leap_device_stack` + hand-built frames | **Learning / fuzz** — not the production porting path |

### PD / MGMT audit (May 2026)

| File | Duplicate sequence / ownership logic? | Notes |
| --- | --- | --- |
| `linux_loopback/device_main.c` | No | All protocol handling via `leap_device_stack_process_frame` + `tick` |
| `linux_loopback/controller_main.c` | No | Bootstrap, PD, DIAG via `leap_controller_stack` |
| `linux_loopback/hub_main.c` | No | Discovery table + `leap_controller_session_hub` |
| `win_l2/controller_main.c` | No | Bootstrap, PD, DIAG via `leap_controller_stack` |
| `win_l2/device_main.c` | No | All protocol handling via `leap_device_stack_process_frame` + `tick` |
| `device_minimal/main.c` | No | Uses stack for all frame processing; local `build_frame()` is transport simulation only |

## Linux loopback

See [linux_loopback/README.md](linux_loopback/README.md).

## Windows L2 (Npcap)

See [win_l2/README.md](win_l2/README.md). Build with `-DLEAP_BUILD_WIN_L2=ON`.

## Windows wire smoke (Npcap loopback)

See [win_smoke/README.md](win_smoke/README.md). Build with `-DLEAP_BUILD_WIN_SMOKE=ON`.
Single-process validation when two-process L2 is impractical on one NIC.

## ClearCore device host

See [platforms/clearcore/README.md](../platforms/clearcore/README.md). LEAP `leap_device_stack` on Teknic ClearCore via ProjectTemplate + lwIP raw hook (`0x88B6`).

## Planned examples

| Example | Description |
| --- | --- |
| `controller_minimal/` | In-memory discovery + session open + cyclic PD (no sockets) |
| `vector_replay/` | Packet replay utility driven by `LEAP_GOLDEN_FRAME_VECTORS.md` |
