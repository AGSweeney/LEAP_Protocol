# LEAP Device Host Performance Checklist

Cross-platform roadmap for sub-millisecond PD exchange latency. Use this
document when porting optimizations from the reference ClearCore firmware to
other device hosts.

Checklist version: `LEAP_DEVICE_PERF_CHECKLIST_VERSION` in
[`leap_core/inc/leap/leap_device_host_perf.h`](../leap_core/inc/leap/leap_device_host_perf.h).

Status as of June 2026.

## Measurement vs wire RTT

| Metric | Source | Meaning |
| --- | --- | --- |
| **Wire RTT** | Controller PD stats (`network_rtt_*`) | Round-trip on the wire including host OS scheduling |
| **Device reply latency** | DIAG `LAST_REPLY_LATENCY_US` (0x0015) / `timing.last_reply_latency_us` | Device CPU time from RX to TX reply only |

Soak SLOs today validate wire RTT. Device reply latency is the firmware gate for
sub-ms work.

**Target gates (soak, 10 ms cycle):**

- Device reply p99: **< 350 µs** (`LEAP_DEVICE_PERF_TARGET_DEVICE_REPLY_P99_US`)
- Wire RTT average: **< 800 µs** (`LEAP_DEVICE_PERF_TARGET_WIRE_RTT_AVG_US`)

## Checklist

| ID | Item | ClearCore | KC868-A16 | GL-C-618WL | BeagleBone | win_l2 | linux loopback |
| --- | --- | --- | --- | --- | --- | --- | --- |
| M0 | Device reply metrics in soak UI/logs | **done** | pending | pending | pending | pending | pending |
| M1a | No main-loop ms sleep when RX idle | **done** | pending | pending | N/A | N/A | N/A |
| M1b | GPIO `Mode()` once; hot path `State()` on change | **done** | pending | pending | partial | N/A | N/A |
| M1c | `outputs_dirty` on PD binding | **done** | pending | pending | **done** | **done** | **done** |
| M1d | TX buffer pool (no per-reply `pbuf_alloc`) | **reverted** | pending | pending | pending | pending | pending |
| M1e | No USB/trace on PD hot path (Release) | **done** | pending | pending | N/A | N/A | N/A |
| M1f | Fast `service_id` peek (no full parse) | **done** | pending | pending | pending | pending | pending |
| M1g | PD-only input refresh before stack | **done** | **done** | pending | N/A | N/A | N/A |
| M2a | PD EXCHANGE fast path (bypass full stack) | pending | pending | pending | pending | pending | pending |
| M2b | Sampled DIAG/stats off hot path | pending | pending | pending | pending | pending | pending |
| M3 | Sub-ms wire RTT average validated | pending | pending | pending | pending | pending | pending |

**Legend:** done = implemented in tree; partial = some pieces; N/A = host model
does not apply (e.g. simulated I/O, blocking recv loop).

## Reference implementations

### M0 — Device reply metrics

- Studio diagnostics: `leap_studio_qt/src/ui/DiagnosticsFormat.cpp` (`last_reply_latency_us`)
- I/O bench table: `leap_studio_qt/src/ui/MainWindow.cpp` (`device_reply_*` rows)
- DIAG poll during soak: `leap_cli/conformance/leap_conformance_win_io.c`
- Counter: `LEAP_COUNTER_LAST_REPLY_LATENCY_US` in `leap_protocol.h`

### M1 — Bring-up / hot-path hygiene (ClearCore)

| Item | File(s) |
| --- | --- |
| M1a | `platforms/clearcore/LeapDeviceFirmware/main.cpp` |
| M1b,c | `clearcore_leap_io.cpp`, `clearcore_leap_host.c` |
| M1d | `clearcore_leap_eth.c` (pool reverted — lwIP frees TX pbuf; pool broke HELLO_REPLY) |
| M1e | `clearcore_leap_trace.cpp`, `leap_device_host_perf.h` (`LEAP_DEVICE_HOST_TRACE_ENABLE`) |
| M1f | `clearcore_leap_host.c` → `leap_device_frame_peek_service_id()` |
| M1g | `clearcore_leap_host.c` (`clearcore_leap_slot_needs_input_refresh`) |

### M1 — Other platforms (port from ClearCore)

| Platform | Host entry | Notes |
| --- | --- | --- |
| Kincony KC868-A16 | `platforms/Espressif/Kincony/KC868-A16/main/leap_host.c` | Already has PD input refresh; add dirty, peek, trace guard |
| GL-C-618WL | `platforms/Espressif/GL-C-618WL/main/leap_host.c` | Same as KC868 |
| BeagleBone | `platforms/TI/BeagleBone/leap_led_device/main.c` | Has `outputs_dirty`; add peek, GPIO change-only |
| win_l2 device | `leap_cli/win_l2/leap_win_device_io.c` | Shadow I/O; add peek in recv path |
| linux loopback | `examples/linux_loopback/leap_linux_io.c` | Shadow I/O; add peek in `device_main.c` |

## Validation procedure

1. Flash/build updated device firmware.
2. Studio → Conformance → I/O bench, 10 ms soak, known peer.
3. Compare **wire_rtt_avg** vs **device_reply_last** in the stats table.
4. Diagnostics tab: **Last reply latency** should trend below wire RTT.
5. If device reply < 500 µs but wire RTT > 1 ms, tune controller host (WinPcap)
   or network path — not firmware.

## Milestone 2+ (not yet implemented)

- **PD fast path:** Prebuilt `EXCHANGE_REPLY` template; service handler dispatches
  on `message_type` without full `leap_device_stack_process_frame` for bench frames.
- **Sampled DIAG:** Update `LAST_REPLY_LATENCY_US` every N exchanges, not every reply.
- **Freerun outliers:** Windows scheduling spikes; I/O bench uses **p99 ≤ 2000 µs**
  plus **max ≤ 5000 µs** on freerun (paced soak still uses max ≤ 1500 µs).
  Soak bench disables cyclic `stats_log_interval` to reduce host jitter.

## Related docs

- [`LEAP_MULTI_PEER_NOTES.md`](LEAP_MULTI_PEER_NOTES.md) — `outputs_dirty`, PD telemetry
- [`LEAP_PROTOCOL_SPECIFICATION.md`](LEAP_PROTOCOL_SPECIFICATION.md) — frame layout for peek offset
