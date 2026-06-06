# `tests`

Protocol conformance and regression tests.

Build and run (local **`build/`** tree — gitignored; see [docs/BUILD.md](../docs/BUILD.md)):

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
# or: ./build/leap_tests
```

Windows: `.\build.ps1 -Test` → `build-win\Release\leap_tests.exe`.

**116 tests** on Windows; **115 on Linux** (Linux skips the two Windows monotonic-clock tests; Windows skips the live `lo` link-query test). Last verified May 2026.

## Test suites

| Suite | File | Tests | Description |
| --- | --- | ---: | --- |
| CRC | `test_crc.c` | 2 | CRC-16/XMODEM and CRC-32C check vectors |
| Frame vectors | `test_frame_vectors.c` | 8 | Golden wire vectors, negative CRC/length cases |
| Frame roundtrip | `test_frame_roundtrip.c` | 3 | Build → parse → header/payload compare |
| Frame fuzz | `test_frame_fuzz.c` | 1 | Single-byte mutations; parser must not crash |
| Frame fragment | `test_frame_fragment.c` | 2 | Fragmented frames rejected at service layers |
| MGMT device | `test_mgmt_device.c` | 15 | State machine, ownership, lease, watchdog, spoof |
| MGMT boundaries | `test_mgmt_boundaries.c` | 4 | Lease clamp, tick edge cases, no side effects on reject |
| MGMT process | `test_mgmt_process.c` | 7 | Frame parse → MGMT dispatch integration |
| MGMT controller | `test_mgmt_controller.c` | 3 | Controller-side MGMT builders and helpers |
| PD device | `test_pd_device.c` | 8 | LEAP-PD ownership gate and lease refresh |
| PD controller | `test_pd_controller.c` | 4 | Cyclic runner, random output bit, exchange validation, stats |
| PD common | `test_pd_common.c` | 6 | Pack/unpack, profile map validation |
| DISC device | `test_disc_device.c` | 3 | HELLO/IDENTIFY replies without state mutation |
| DISC controller | `test_disc_controller.c` | 2 | HELLO broadcast and reply parsing |
| DIR device | `test_dir_device.c` | 3 | SELECT_PROFILE, directory, object read |
| DIAG device | `test_diag_device.c` | 3 | Counters, timing, event log handlers |
| DIAG controller | `test_diag_controller.c` | 3 | READ_COUNTERS / READ_TIMING builders and parsers |
| Controller stack | `test_controller_stack.c` | 9 | Bootstrap FSM, release, replay, session bind, `read_diag` |
| Controller peer | `test_controller_peer.c` | 6 | Discovery table, early exit, MAC parse, foreign-owner |
| Controller sequence | `test_controller_sequence.c` | 5 | Per-peer Ethernet sequence / replay window |
| Session hub | `test_controller_session_hub.c` | 9 | Multi-peer bootstrap, round-robin, parallel, random-peer indexing, isolation |
| Device stack | `test_device_stack.c` | 3 | End-to-end MGMT + PD dispatch, exchange via `leap_frame_write` |
| Comms loss | `test_comms_loss.c` | 2 | Heartbeat prevents lease expiry; loss → SAFE |
| Raw Linux stats | `test_raw_linux_stats.c` | 1–2 | Transport counter reset; live `lo` link query (Linux only) |
| Windows time | `test_win_time.c` | 0–2 | Monotonic clock and sleep accuracy (Windows only) |
| Log | `test_log.c` | 1 | Timestamp formatting (`leap_log_format_timestamp`) |

## Shared helpers

| Helper | Purpose |
| --- | --- |
| `test_harness.c` | `RUN_TEST`, assertions, pass/fail summary |
| `test_util.c` | Hex decode for golden vectors |
| `leap_test_frame.c` | Frame build, header compare, mutation |

## Categories

| Category | Coverage |
| --- | --- |
| CRC verification | Published check values and golden vector CRCs |
| Header validation | Magic, version, length, header CRC |
| Payload rejection | Bad payload CRC, truncation, fuzz mutations |
| Session/state | Owner lease, watchdog, spoof → SAFE, STEAL_EXPIRED, rollover |
| Padding boundary | Minimum Ethernet payload (vector 6) |
| Fragment policy | Rejected until reassembly engine exists |
| Discovery | HELLO reply identity, early exit at `min_peers`, MAC parse |
| Multi-peer | Session hub round-robin, parallel, random-peer lap, foreign-owner skip, bootstrap isolation |
| DIAG | Device handlers + controller read path round-trip |
| Logging | Relative T+ timestamps via injectable monotonic clock |
| Transport | Link stats reset (`leap_raw_linux`; stubs compile on Windows) |
| Platform | Windows monotonic time (`leap_win_time`) |

## CI

GitHub Actions runs `ctest` on **Linux** and **Windows**. Wire smoke scripts under `tools/ci/` are **manual** only.
