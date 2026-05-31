# `tests`

Protocol conformance and regression tests.

Run all suites:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
# or: ./build/leap_tests
```

## Test Suites

| Suite | File | Description |
| --- | --- | --- |
| CRC | `test_crc.c` | CRC-16/XMODEM and CRC-32C check vectors |
| Frame vectors | `test_frame_vectors.c` | Golden wire vectors, negative CRC/length cases |
| Frame roundtrip | `test_frame_roundtrip.c` | Build → parse → header/payload compare |
| Frame fuzz | `test_frame_fuzz.c` | Single-byte mutations; parser must not crash |
| Frame fragment | `test_frame_fragment.c` | Fragmented frames rejected at service layers |
| MGMT device | `test_mgmt_device.c` | State machine, ownership, lease, watchdog, spoof |
| MGMT boundaries | `test_mgmt_boundaries.c` | Lease clamp, tick edge cases, no side effects on reject |
| MGMT process | `test_mgmt_process.c` | Frame parse → MGMT dispatch integration |
| PD device | `test_pd_device.c` | LEAP-PD ownership gate and lease refresh |
| DISC device | `test_disc_device.c` | HELLO/IDENTIFY replies without state mutation |
| Device stack | `test_device_stack.c` | End-to-end MGMT + PD dispatch |

## Shared Helpers

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
| Discovery | HELLO reply identity and non-mutation guarantee |
