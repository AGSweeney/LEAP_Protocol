# Linux Loopback Example

Raw Ethernet LEAP discovery over Linux `AF_PACKET` (development EtherType `0x88B6`).

## Requirements

- Linux with `CAP_NET_RAW` (typically root)
- Two terminals on the same interface (default: `lo`)

## Build

```bash
cmake -S . -B build
cmake --build build
```

Targets (Linux only):

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

Expected: controller sends `HELLO`, device responds with `HELLO_REPLY`.

## Notes

- This example covers discovery only. For full session/PD flow see `device_minimal` (simulated transport).
- On a physical interface, use your NIC name instead of `lo`.
