# `src`

Protocol implementation code.

## Modules

| Directory | Contents |
| --- | --- |
| `crc/` | CRC-16/XMODEM and CRC-32C engines |
| `frame/` | LEAP header parser and serializer (`leap_frame_write`) |
| `services/disc/` | LEAP-DISC device handler (HELLO, IDENTIFY) |
| `services/mgmt/` | LEAP-MGMT device handler and frame dispatch |
| `services/pd/` | LEAP-PD device handler |
| `leap_device_stack.c` | Device-side DISC + MGMT + PD frame dispatch |
| `transport/` | Platform-specific raw Ethernet (`leap_raw_linux.c` on Linux) |
