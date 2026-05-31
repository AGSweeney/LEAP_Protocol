# Linux Loopback Example

Raw Ethernet LEAP over Linux `AF_PACKET` (development EtherType `0x88B6`).

Applications use the **reference stacks** — not ad-hoc MGMT/PD logic:

| Binary | Stack | Role |
| --- | --- | --- |
| `leap_linux_device` | `leap_device_stack` | DISC, DIR, MGMT, PD, DIAG dispatch + tick |
| `leap_linux_controller` | `leap_controller_stack` | Bootstrap to OP, cyclic or single PD |
| `leap_linux_hub` | `leap_controller_session_hub` | Discover, bootstrap all peers, round-robin PD |
| `leap_linux_discover` | peer table | Broadcast HELLO scan (multi-device demo) |

## Flow (default controller run)

1. **Controller** broadcasts `HELLO`
2. **Device** replies with `HELLO_REPLY` (identity, state, supported services incl. DIAG)
3. **Controller** sends `SELECT_PROFILE` (`LEAP-DIR`)
4. **Device** enters `CONFIGURED`, replies `PROFILE_REPLY`
5. **Controller** `OPEN_SESSION` → device `SAFE`
6. **Controller** `SET_STATE → OP`
7. **Controller** PD write or cyclic exchange
8. **Device** applies outputs to I/O shadow; optional DIAG counters increment

The device runs a recv loop with monotonic-time `leap_device_stack_tick()` for
lease/watchdog expiry. Link up/down transitions are logged via
`leap_linux_poll_link_and_log()` each loop iteration.

## Requirements

- **Native Linux** with `CAP_NET_RAW` (typically root)
- Two terminals on the same interface (default: `lo`)

### WSL2 limitation

WSL2 rejects `AF_PACKET` bind with `ENODEV` even on `lo`. Unit tests build in
WSL; wire examples need bare metal Linux or a VM.

### CI vs local wire smoke

GitHub Actions runs **unit tests only** (network namespace blocks veth setup).
End-to-end wire tests are **manual** on native Linux:

```bash
cmake -S . -B build && cmake --build build
LEAP_BUILD_DIR=build tools/ci/wire_smoke_lo.sh
LEAP_BUILD_DIR=build tools/ci/wire_smoke_discover_lo.sh
```

When `CI=true` or `LEAP_FORCE_VETH=1`, smoke scripts use an isolated veth bridge
(if the host allows it). Set `LEAP_SKIP_WIRE_SMOKE=1` to skip.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Targets: `leap_linux_device`, `leap_linux_controller`, `leap_linux_discover`, `leap_linux_hub`

## Run

Terminal 1 (device):

```bash
sudo ./build/leap_linux_device lo
```

Terminal 2 (controller — single PD write, stats at end):

```bash
sudo ./build/leap_linux_controller lo
```

### Cyclic PD (default 100 ms; Ctrl+C to stop)

```bash
sudo ./build/leap_linux_controller --cyclic lo
sudo ./build/leap_linux_controller --cyclic-ms 50 lo
sudo ./build/leap_linux_controller --cyclic --exchange lo
```

Periodic stats use `--stats-interval N` (default 100 cycles when `--cyclic` is set).
Final summary includes **latency**, **cycle jitter**, **lost frames** (exchange
timeouts), and reply reject counts.

Example stats line:

```text
PD stats: cycles=100 ok=100 ... lost=0 ... jitter last=1200 avg=800 max=3500 us target=100 ms ...
```

### Lease expiry demo

```bash
sudo ./build/leap_linux_controller --lease-demo lo
```

2 s lease, reaches `OP`, idles 3 s without heartbeat/PD. Watch the **device** log.

### Diagnostics read (`--diag`)

After bootstrap (and optionally after a PD run), issue `READ_COUNTERS` +
`READ_TIMING` via `leap_controller_stack_read_diag()`:

```bash
# DIAG only (no PD write)
sudo ./build/leap_linux_controller --diag lo

# PD write then DIAG
sudo ./build/leap_linux_controller lo --diag

# Cyclic PD, then DIAG on exit (Ctrl+C)
sudo ./build/leap_linux_controller --cyclic --diag lo
```

### Multi-device discovery

```bash
sudo ./build/leap_linux_discover lo
```

Broadcasts HELLO and prints discovered peer MACs (no bootstrap).

### Multi-device session hub

Run one device per peer (separate terminals or hosts on the same segment):

```bash
# Terminal 1 — device A (use distinct interfaces/MACs on real hardware)
sudo ./build/leap_linux_device lo

# Terminal 2 — optional second device if your setup supports two MACs on lo
# sudo ./build/leap_linux_device eth0

# Terminal 3 — hub controller: discover → bootstrap_table → round-robin PD
sudo ./build/leap_linux_hub lo
sudo ./build/leap_linux_hub --scan-ms 5000 --cyclic-ms 100 --exchange lo
```

Foreign-owned peers (another controller’s `active_owner_mac` in HELLO) are skipped
by default. Ctrl+C stops round-robin and releases all hub sessions.

Hub does not yet expose a `--diag` flag; use `leap_linux_controller --diag` for
single-peer DIAG reads.

## Example device output

```text
received HELLO
sent reply (service=0x0002 message=0x0002)
received SELECT_PROFILE
profile selected -> CONFIGURED
received OPEN_SESSION
ownership granted -> SAFE
received SET_STATE -> state now 4
received PD WRITE_ENDPOINT (state=4)
I/O shadow: outputs=0x0015 inputs=0x0004 (live)
```

After `--lease-demo`:

```text
tick: lease/watchdog expired -> SAFE (state=3)
I/O shadow: safe outputs active (outputs=0x0000)
```

## Wireshark / tcpdump

```bash
sudo tcpdump -i lo -XX ether proto 0x88b6
```

Wireshark filter: `eth.type == 0x88b6`. Load `tools/wireshark/leap_dissector.lua` for decode.

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
sudo ./build/leap_linux_controller --promisc eth0
```

Flags can appear before or after the interface name.

## Notes

- Test on `lo` first; use `--promisc eth0` when the switch does not flood unicast.
- Device I/O shadow is simulated in-process (no real GPIO).
- PD send retries up to 3 times on transient transport failure.
- Received frames filter to local MAC, broadcast, and multicast unless promiscuous.
