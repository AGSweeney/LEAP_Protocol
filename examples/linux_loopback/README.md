# Linux Loopback Example

Raw Ethernet LEAP discovery + directory + MGMT + PD over Linux `AF_PACKET`
(development EtherType `0x88B6`).

## Flow

1. **Controller** broadcasts `HELLO`
2. **Device** replies with `HELLO_REPLY` (identity + state)
3. **Controller** sends `SELECT_PROFILE` (`LEAP-DIR`)
4. **Device** replies with `PROFILE_REPLY` and enters `CONFIGURED`
5. **Controller** sends `OPEN_SESSION` (owner lease request)
6. **Device** replies with `OPEN_SESSION_REPLY` and enters `SAFE`
7. **Controller** sends `SET_STATE -> OP`
8. **Device** replies with `STATE_REPLY` — device is now in `OP`
9. **Controller** sends `PD WRITE_ENDPOINT` (digital outputs)
10. **Device** updates its I/O shadow and logs applied outputs

The device runs a 100 ms recv loop with monotonic-time `tick()` for lease/watchdog
expiry.

Controller logic uses `leap_mgmt_controller` and `leap_dir_controller` from
`leap_core`.

## Requirements

- **Native Linux** with `CAP_NET_RAW` (typically root)
- Two terminals on the same interface (default: `lo`)

### WSL2 limitation

WSL2 rejects `AF_PACKET` bind with `ENODEV` even on `lo`. Build and unit tests
work in WSL; the wire example requires bare metal Linux or a VM with a real
network stack. The CI wire-smoke script auto-skips on WSL2.

### CI wire smoke

GitHub Actions runs end-to-end tests after unit tests. Hosted runners expose
`lo` but often reject `AF_PACKET` bind with `ENODEV`, so CI smoke scripts
create an isolated **veth + bridge** pair automatically when `CI=true`:

```bash
LEAP_BUILD_DIR=build tools/ci/wire_smoke_lo.sh
LEAP_BUILD_DIR=build tools/ci/wire_smoke_discover_lo.sh
```

Set `LEAP_SKIP_WIRE_SMOKE=1` to skip locally. Set `LEAP_FORCE_VETH=1` to use
the veth bridge on any host (useful when `lo` fails outside CI).

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

### Cyclic PD mode (default 100 ms, heartbeat every 10 cycles or half-lease)

```bash
sudo ./build/leap_linux_controller --cyclic lo
sudo ./build/leap_linux_controller --cyclic-ms 50 lo
```

Press Ctrl+C to stop. Controller rotates output pattern `0x0001`, `0x0002`, …

### Lease expiry demo

```bash
sudo ./build/leap_linux_controller --lease-demo lo
```

Uses a 2 s lease, reaches `OP`, then idles 3 s without heartbeat/PD. Watch the
**device** terminal.

## Example terminal output

**Device** (after default controller run):

```text
received HELLO
sent reply (service=0x0002 message=0x0002)
received SELECT_PROFILE
profile selected -> CONFIGURED
sent reply (service=0x0003 message=0x0008)
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
cyclic PD mode (100 ms) — Ctrl+C to stop
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
sudo ./build/leap_linux_controller --cyclic-ms 50 lo
sudo ./build/leap_linux_controller --cyclic --exchange lo
sudo ./build/leap_linux_controller --lease-demo lo
sudo ./build/leap_linux_controller --stats-interval 50 --cyclic lo
sudo ./build/leap_linux_device --stats lo
sudo ./build/leap_linux_controller --promisc eth0   # IFF_PROMISC on shared LAN
```

Flags can appear before or after the interface name.

Transport errors print `strerror()` details when `errno` is set (recv timeouts are
silent). `open` on WSL2 prints an additional hint.

## Notes

- Test on `lo` first; use `--promisc eth0` on a physical NIC when the switch does
  not flood unicast to your port.
- The device I/O shadow is simulated in-process (no real GPIO).
- PD send uses up to 3 retries on transient transport failure.
- Received frames are filtered to local MAC, broadcast, and multicast unless
  promiscuous mode is enabled on a shared segment.
