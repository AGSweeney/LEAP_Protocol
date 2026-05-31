# `src`

Protocol implementation code.

## Modules

| Directory | Contents |
| --- | --- |
| `crc/` | CRC-16/XMODEM and CRC-32C engines |
| `frame/` | LEAP header parser and serializer |
| `services/` | Service handlers: DISC, MGMT, DIR, PD, DIAG |
| `leap_device_stack.c` | Device-side MGMT + PD frame dispatch |
| `transport/` | Platform-specific raw Ethernet send/receive |
