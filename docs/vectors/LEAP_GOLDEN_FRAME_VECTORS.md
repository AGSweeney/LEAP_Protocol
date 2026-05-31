<!--
Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
SPDX-License-Identifier: MIT

Purpose: Golden LEAP frame vectors for interoperability testing.
-->

# LEAP Golden Frame Vectors

Deterministic test vectors for LEAP v1.0. All byte streams are aligned to the wire contract defined in `inc/leap/leap_protocol.h` and `docs/LEAP_PROTOCOL_SPECIFICATION.md`.

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
| `e0 01` | `0x01E0` | `digital_inputs` (channels 6–9 active; bits 5–8 set) |
| `15 00` | `0x0015` | `digital_outputs` (channels 1, 3, 5 active) |
| `00 00` | `0x0000` | `io_status` (OK) |
| `f1` | `241` | `v_field_supply` (24.1 V) |
| `00` | `0x00` | `reserved0` |

Bytes 6–7 of the payload are `endpoint_flags = 0x0005` (`APPLY_OUTPUTS | TIMESTAMP_VALID`). These are the two active flag bits in `LeapEndpointDataHeader.endpoint_flags` as defined in `leap_protocol.h`.

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

> **REVIEW before v1.0 tag:** Header byte 7 (flags) is `0x0A` = `RESPONSE (0x02) | BROADCAST (0x08)`.
> `HELLO_REPLY` is defined as "Device **unicast** to controller" (§9). The `BROADCAST` flag MUST NOT
> be set on a unicast frame (§6.2). Correct value is `0x02`. When fixed, `header_crc16` must be
> recomputed over the corrected 32-byte header and both the scenario flags line and the header hex
> string below must be updated.

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

## 7. Bad-Header-CRC Negative Test Vector

Scenario:

- Based on Section 2 (`DIGITAL_IO_16X16`) valid frame
- Same payload bytes and `payload_length`
- `payload_crc32c` field in the header intentionally corrupted to `0xC723066B`

The `payload_crc32c` field occupies header bytes 28–31, which fall within the
`header_crc16` coverage window (all `header_length` bytes). Corrupting this field
therefore invalidates the `header_crc16` check. A conformance receiver MUST
reject this frame at header integrity validation before ever inspecting the
payload. This vector tests the header-CRC rejection path.

### 7.1 Header Bytes (`32` bytes, bad `header_crc16` due to corrupted `payload_crc32c` field)

```text
4c45415001002001100001001d2c3b4aed030000d6030000280063146b0623c7
```

### 7.2 Payload Bytes (`40` bytes, unchanged from Section 2)

```text
1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100
```

## 8. Bad-Payload-CRC Negative Test Vector

Scenario:

- Based on Section 2 (`DIGITAL_IO_16X16`) valid frame
- Header is byte-for-byte identical to Section 2, including a valid `header_crc16`
  and the original correct `payload_crc32c = 0x38DCF994`
- Final payload byte corrupted from `0x00` to `0xFF` to simulate a payload
  transmission error

The header CRC is valid (header bytes are unchanged). The receiver MUST pass
header validation and then compute the CRC-32C of the received payload bytes. The
computed CRC will not match the stored `payload_crc32c = 0x38DCF994` because the
payload has been altered. The receiver MUST reject the frame at payload integrity
validation. This vector tests the payload-CRC rejection path (§6.1).

### 8.1 Header Bytes (`32` bytes, valid, unchanged from Section 2)

```text
4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38
```

### 8.2 Payload Bytes (`40` bytes, last byte corrupted `0x00` → `0xFF`)

```text
1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f1ff
```

## 9. Notes

- Byte streams cover the LEAP frame (header + payload, plus transport padding where shown). Ethernet destination/source MAC and EtherType fields are not included unless explicitly stated.
- CRC coverage is limited to the true payload bytes declared by `payload_length`. Transport padding bytes are excluded from both `payload_crc32c` computation and receiver CRC validation.
- The profile ID in Section 2 (`0x00010002`) matches `LEAP_PROFILE_DIGITAL_IO_16X16` in `inc/leap/leap_protocol.h` and the profile registry table in `docs/LEAP_PROTOCOL_SPECIFICATION.md`.
- Section 7 tests the header-CRC rejection path. Section 8 tests the payload-CRC rejection path. Both are required by §6.1 of the specification.
