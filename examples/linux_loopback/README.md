# Linux Loopback Example

Raw Ethernet LEAP discovery + MGMT over Linux `AF_PACKET` (development EtherType `0x88B6`).

## Flow

1. **Controller** broadcasts `HELLO`
2. **Device** replies with `HELLO_REPLY` (identity + state)
3. **Controller** sends `OPEN_SESSION` (owner lease request)
4. **Device** replies with `OPEN_SESSION_REPLY` and enters `SAFE`
5. **Controller** sends `SET_STATE -> OP`
6. **Device** replies with `STATE_REPLY` — device is now in `OP`

The device runs a 100 ms recv loop with monotonic-time `tick()` for lease/watchdog expiry logging.

## Requirements

- Linux with `CAP_NET_RAW` (typically root)
- Two terminals on the same interface (default: `lo`)

## Build

```bash
cmake -S . -B build
cmake --build build
```

Targets:

- `leap_linux_device`
- `leap_linux_controller`

## Run

Terminal 1 (device):

```bash
sudo ./build/leap_linux_device lo
```

Terminal 2 (controller):

```bash
sudo ./build/leap_linux_controller lo
```

Expected controller output ends with `MGMT flow complete — device is in OP with active owner lease`.

## Options

```bash
sudo ./build/leap_linux_device eth0
sudo ./build/leap_linux_controller eth0
```

Transport errors print `strerror()` details when `errno` is set (timeouts are silent).

## Notes

- Test on `lo` first; validate on a physical NIC before relying on MAC filtering behavior.
- PD writes are not yet included in this example — see `device_minimal` for simulated PD.
