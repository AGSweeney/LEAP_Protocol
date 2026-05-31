# Linux Loopback Example

Raw Ethernet LEAP discovery + MGMT + PD over Linux `AF_PACKET` (development EtherType `0x88B6`).

## Flow

1. **Controller** broadcasts `HELLO`
2. **Device** replies with `HELLO_REPLY` (identity + state)
3. **Controller** sends `OPEN_SESSION` (owner lease request)
4. **Device** replies with `OPEN_SESSION_REPLY` and enters `SAFE`
5. **Controller** sends `SET_STATE -> OP`
6. **Device** replies with `STATE_REPLY` — device is now in `OP`
7. **Controller** sends `PD WRITE_ENDPOINT` (digital outputs)
8. **Device** updates its I/O shadow and logs applied outputs

The device runs a 100 ms recv loop with monotonic-time `tick()` for lease/watchdog expiry.

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

Terminal 2 (controller — single PD write):

```bash
sudo ./build/leap_linux_controller lo
```

### Cyclic PD mode (~100 ms, with heartbeat every 10 cycles)

```bash
sudo ./build/leap_linux_controller --cyclic lo
```

Press Ctrl+C to stop. Controller rotates output pattern `0x0001`, `0x0002`, … and sends `HEARTBEAT` every ~1 s.

### Lease expiry demo

```bash
sudo ./build/leap_linux_controller --lease-demo lo
```

Uses a 2 s lease, reaches `OP`, then idles 3 s without heartbeat/PD. Watch the **device** terminal.

## Example terminal output

**Device** (after default controller run):

```text
received HELLO
sent reply (service=0x0002 message=0x0002)
received OPEN_SESSION
ownership granted -> SAFE
sent reply (service=0x0001 message=0x0002)
received SET_STATE -> state now 4
sent reply (service=0x0001 message=0x0006)
received PD WRITE_ENDPOINT (state=4)
I/O shadow: outputs=0x0015 inputs=0x0004 (live)
PD outputs applied: endpoint=0x0010 seq=1001
```

**Device** (after `--lease-demo`):

```text
tick: lease/watchdog expired -> SAFE (state=3)
I/O shadow: safe outputs active (outputs=0x0000)
```

**Controller** (cyclic):

```text
cyclic PD mode (~100 ms) — Ctrl+C to stop
cyclic PD seq=1000 outputs=0x0001
sent HEARTBEAT (lease refresh)
```

## Wireshark / tcpdump

Capture LEAP frames on loopback (EtherType `0x88B6`):

```bash
sudo tcpdump -i lo -XX ether proto 0x88b6
```

Or in Wireshark, filter: `eth.type == 0x88b6`

## Options

```bash
sudo ./build/leap_linux_device eth0
sudo ./build/leap_linux_controller eth0
sudo ./build/leap_linux_controller --cyclic lo
sudo ./build/leap_linux_controller --lease-demo lo
```

Flags can appear before or after the interface name.

Transport errors print `strerror()` details when `errno` is set (recv timeouts are silent).

## Notes

- Test on `lo` first; validate on a physical NIC before relying on MAC behavior.
- The device I/O shadow is simulated in-process (no real GPIO).
- PD send uses up to 3 retries on transient transport failure.
