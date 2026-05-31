<!--
Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
SPDX-License-Identifier: MIT

Purpose: Golden LEAP frame vectors for interoperability testing.
-->

# LEAP Golden Frame Vectors

This document provides deterministic vectors aligned to `leap_protocol.h` and
`LEAP_PROTOCOL_SPECIFICATION.md`.

## 1. CRC Check Vectors

- CRC-16/XMODEM over ASCII `123456789` = `0x31C3`
- CRC-32C over ASCII `123456789` = `0xE3069283`

## 2. Golden LEAP-PD Frame (`DIGITAL_IO_16X16`)

Scenario:

- EtherType: `0x88B6` (development default)
- Service ID: `0x0010` (`LEAP_SERVICE_PD`)
- Message Type: `0x0001` (`LEAP_PD_WRITE_ENDPOINT`)
- Session ID: `0x4A3B2C1D`
- Sequence/Ack: `1005 / 982`
- Profile ID: `0x00010002` (`DIGITAL_IO_16X16`)
- Endpoint ID: `0x0010` (`LEAP_ENDPOINT_DIGITAL_OUTPUTS`)
- Payload layout: `LeapEndpointDataHeader` (`32` bytes) + `LeapProfileDigital16x16` (`8` bytes)
- True LEAP payload length: `40` bytes

Derived integrity values:

- `payload_crc32c = 0x38DCF994`
- `header_crc16 = 0x1463` (computed with header CRC field zeroed)

### 2.1 Header Bytes (`32` bytes)

```text
4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38
```

### 2.2 Payload Bytes (`40` bytes)

```text
1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100
```

Field-by-field decode of `LeapEndpointDataHeader` (`32` bytes) followed by `LeapProfileDigital16x16` (`8` bytes):

| Bytes | Value | Field |
| --- | --- | --- |
| `10 00` | `0x0010` | `endpoint_id` (`LEAP_ENDPOINT_DIGITAL_OUTPUTS`) |
| `00 00` | `0x0000` | `endpoint_offset` |
| `08 00` | `8` | `data_length` |
| `05 00` | `0x0005` | `endpoint_flags` (`APPLY_OUTPUTS \| TIMESTAMP_VALID`) |
| `ed 03 00 00` | `1005` | `process_sequence` |
| `e8 03 00 00` | `1000` | `cycle_time_us` |
| `00 c4 b3 a2 96 01 00 00` | `0x00000196A2B3C400` | `controller_timestamp_us` |
| `88 13 00 00` | `5000` | `max_frame_age_us` |
| `02 00 01 00` | `0x00010002` | `profile_id` (`DIGITAL_IO_16X16`) |
| `e0 01` | `0x01E0` | `digital_inputs` (channels 6–8 active) |
| `15 00` | `0x0015` | `digital_outputs` (channels 1, 3, 5 active) |
| `00 00` | `0x0000` | `io_status` (OK) |
| `f1` | `241` | `v_field_supply` (24.1 V) |
| `00` | `0x00` | `reserved0` |

`0x0500` in the earlier raw notation was bytes 6–7 of the payload, which is `endpoint_flags = 0x0005`. These are the `APPLY_OUTPUTS` and `TIMESTAMP_VALID` flag bits from `LeapEndpointDataHeader` and are fully accounted for by the struct definition in `leap_protocol.h`.

### 2.3 Full LEAP Frame Bytes (Header + Payload, `72` bytes)

```text
4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc381000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100
```

## 3. Golden Discovery `HELLO_REPLY` Vector

Scenario:

- EtherType: `0x88B6` (development default)
- Service ID: `0x0002` (`LEAP_SERVICE_DISC`)
- Message Type: `0x0002` (`LEAP_DISC_HELLO_REPLY`)
- Session ID: `0x00000000` (discovery context)
- Sequence/Ack: `10 / 0`
- Reply body: `LeapHelloReply` (`44` bytes) followed by 5 supported service IDs (`10` bytes)
- True LEAP payload length: `54` bytes

Derived integrity values:

- `payload_crc32c = 0xDC3B5B4A`
- `header_crc16 = 0xDE8E`

### 3.1 Header Bytes (`32` bytes)

```text
4c4541500100200a02000200000000000a0000000000000036008ede4a5b3bdc
```

### 3.2 Payload Bytes (`54` bytes)

```text
66778899aabb341201efcdab040302010100100003000000020001000200010002000500000000000000010001000200030010002000
```

## 4. Golden `OPEN_SESSION_REPLY` Vector

Scenario:

- Service ID: `0x0001` (`LEAP_SERVICE_MGMT`)
- Message Type: `0x0002` (`LEAP_MGMT_OPEN_SESSION_REPLY`)
- Session ID: `0x4A3B2C1D`
- Sequence/Ack: `110 / 0`
- True LEAP payload length: `24` bytes (`LeapOpenSessionReply`)

Derived integrity values:

- `payload_crc32c = 0xBA752FDA`
- `header_crc16 = 0x255D`

### 4.1 Header Bytes (`32` bytes)

```text
4c45415001002003010002001d2c3b4a6e0000000000000018005d25da2f75ba
```

### 4.2 Payload Bytes (`24` bytes)

```text
1d2c3b4a20a10700a08601000500030066778899aabb0000
```

## 5. Golden Error Response Vector

Scenario:

- Service ID: `0x0010` (`LEAP_SERVICE_PD`)
- Message Type: `0x0001` (`LEAP_PD_WRITE_ENDPOINT`)
- Flags include `RESPONSE | ERROR`
- True LEAP payload length: `16` bytes (`LeapErrorPayload`)

Derived integrity values:

- `payload_crc32c = 0x0592F3AD`
- `header_crc16 = 0xC71F`

### 5.1 Header Bytes (`32` bytes)

```text
4c45415001002007100001001d2c3b4ab1040000b004000010001fc7adf39205
```

### 5.2 Payload Bytes (`16` bytes)

```text
07000100b00400001000000008000000
```

## 6. Small-Frame Padding Vector

Scenario:

- Service ID: `0x0001` (`LEAP_SERVICE_MGMT`)
- Message Type: `0x0005` (`LEAP_MGMT_SET_STATE`)
- True LEAP payload length: `4` bytes
- LEAP bytes before transport padding: `32 + 4 = 36`
- Required transport padding bytes to reach minimum transmitted Ethernet payload (`50`): `14`

Derived integrity values:

- `payload_crc32c = 0x33457A34`
- `header_crc16 = 0x8831`

### 6.1 Header Bytes (`32` bytes)

```text
4c45415001002001010005001d2c3b4a140500000000000004003188347a4533
```

### 6.2 True Payload Bytes (`4` bytes)

```text
04000000
```

### 6.3 Full Transmitted Ethernet Payload (LEAP + zero padding, `50` bytes)

```text
4c45415001002001010005001d2c3b4a140500000000000004003188347a4533040000000000000000000000000000000000
```

## 7. Bad-CRC Negative Test Vector

Scenario:

- Based on Section 2 (`DIGITAL_IO_16X16`) valid frame
- Same payload bytes and `payload_length`
- `payload_crc32c` field intentionally corrupted for parser rejection testing

Corrupted payload CRC field value: `0xC723066B`

### 7.1 Header Bytes (`32` bytes, intentionally bad payload CRC field)

```text
4c45415001002001100001001d2c3b4aed030000d6030000280063146b0623c7
```

### 7.2 Payload Bytes (`40` bytes)

```text
1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100
```

## 8. Notes

- Vector byte streams here are LEAP bytes (header + payload and optional transport
  padding); Ethernet destination/source MAC and EtherType bytes are not included
  unless noted.
- When generating CRCs, include only the true payload bytes (`payload_length`),
  never transport padding.
- Profile ID in Section 2 (`0x00010002`) matches
  `LEAP_PROFILE_DIGITAL_IO_16X16` in `leap_protocol.h` and the profile registry
  table in `LEAP_PROTOCOL_SPECIFICATION.md`.
