/*
 * leap_protocol.h
 *
 * LEAP - Lightweight Ethernet Application Protocol
 * Wire-format constants and packed structures.
 *
 * Draft v1.0
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Notes:
 * - LEAP uses raw Ethernet frames on an isolated machine network.
 * - Multi-byte LEAP protocol fields are little-endian on the wire, including
 *   all packed payload structures in raw Ethernet frames. This is intentional.
 * - This header defines wire-layout structures only. It does not implement
 *   CRC calculation, frame parsing, ownership policy, or endpoint application.
 * - CRC-16/XMODEM and CRC-32C parameters are fixed by this header and the
 *   protocol specification; use published vectors for interoperability testing.
 * - Ethernet transports must pad small frames to LEAP_MIN_ETHERNET_PAYLOAD.
 *   payload_length and payload_crc32c cover only the true LEAP payload bytes.
 * - Do not place pointers, bool, size_t, enums inside wire payload structs.
 */

#ifndef LEAP_PROTOCOL_H
#define LEAP_PROTOCOL_H

#include <stdint.h>

#if defined(__cplusplus)
  #define LEAP_STATIC_ASSERT static_assert
#elif defined(_MSC_VER)
  #define LEAP_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
  #define LEAP_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#elif defined(__GNUC__) || defined(__clang__)
  #define LEAP_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
  #define LEAP_STATIC_ASSERT_LINE(line, cond, msg) \
      typedef char static_assertion_##line[(cond) ? 1 : -1]
  #define LEAP_STATIC_ASSERT(cond, msg) \
      LEAP_STATIC_ASSERT_LINE(__LINE__, cond, msg)
#endif

#if defined(_MSC_VER)
  #define LEAP_PACKED_BEGIN __pragma(pack(push, 1))
  #define LEAP_PACKED_END   __pragma(pack(pop))
  #define LEAP_PACKED
#elif defined(__GNUC__) || defined(__clang__)
  #define LEAP_PACKED_BEGIN
  #define LEAP_PACKED_END
  #define LEAP_PACKED __attribute__((packed))
#else
  #error "Unsupported compiler for LEAP packed wire structs. Add explicit packing macros before use."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Version and Ethernet constants                                             */
/* -------------------------------------------------------------------------- */

#define LEAP_VERSION_MAJOR                 1u
#define LEAP_VERSION_MINOR                 0u

/* CRC-16/XMODEM parameters (header_crc16). */
#define LEAP_CRC16_XMODEM_POLY             0x1021u
#define LEAP_CRC16_XMODEM_INIT             0x0000u
#define LEAP_CRC16_XMODEM_XOR_OUT          0x0000u
#define LEAP_CRC16_XMODEM_REFIN            0u
#define LEAP_CRC16_XMODEM_REFOUT           0u
#define LEAP_CRC16_XMODEM_CHECK_123456789  0x31C3u

/* CRC-32C (Castagnoli) parameters (payload_crc32c). */
#define LEAP_CRC32C_POLY                   0x1EDC6F41u
#define LEAP_CRC32C_INIT                   0xFFFFFFFFu
#define LEAP_CRC32C_XOR_OUT                0xFFFFFFFFu
#define LEAP_CRC32C_REFIN                  1u
#define LEAP_CRC32C_REFOUT                 1u
#define LEAP_CRC32C_CHECK_123456789        0xE3069283u

/* v1 wire marker: ASCII "LEAP" in little-endian uint32_t form. */
#define LEAP_MAGIC_U32                     0x5041454CUL
#define LEAP_MAGIC_B0                      0x4Cu
#define LEAP_MAGIC_B1                      0x45u
#define LEAP_MAGIC_B2                      0x41u
#define LEAP_MAGIC_B3                      0x50u

#define LEAP_HEADER_LENGTH_V1              32u
#define LEAP_MIN_ETHERNET_MTU              1500u
#define LEAP_MIN_ETHERNET_PAYLOAD          50u
#define LEAP_MAX_ETHERNET_PAYLOAD          1500u
#define LEAP_MAX_PAYLOAD_V1                (LEAP_MAX_ETHERNET_PAYLOAD - LEAP_HEADER_LENGTH_V1)
#define LEAP_MIN_PAYLOAD_WITHOUT_PADDING_V1 (LEAP_MIN_ETHERNET_PAYLOAD - LEAP_HEADER_LENGTH_V1)

/*
 * Maximum allowed total_length for a fragmented reassembly group.
 * Implementations MUST NOT accept a fragment group whose total_length exceeds
 * this value; the group MUST be rejected with BAD_LENGTH. Implementations may
 * reduce this limit at build time but MUST NOT increase it without a
 * corresponding increase in static reassembly buffer allocation.
 */
#define LEAP_MAX_REASSEMBLY_SIZE           4096u

/*
 * Development and experimental EtherTypes.
 * A production LEAP EtherType requires formal assignment.
 */
#define LEAP_ETHERTYPE_DEVELOPMENT         0x88B6u
#define LEAP_ETHERTYPE_EXPERIMENTAL_ALT    0x88B5u

#define LEAP_BROADCAST_SESSION_ID          0u
#define LEAP_NO_SESSION_ID                 0u
#define LEAP_NO_ACK_SEQUENCE               0u
#define LEAP_NO_PAYLOAD_CRC32C             0u

/* -------------------------------------------------------------------------- */
/* Header flags                                                               */
/* -------------------------------------------------------------------------- */

#define LEAP_FLAG_ACK_REQUESTED            (1u << 0)
#define LEAP_FLAG_RESPONSE                 (1u << 1)
#define LEAP_FLAG_ERROR                    (1u << 2)
#define LEAP_FLAG_BROADCAST                (1u << 3)
#define LEAP_FLAG_NO_PAYLOAD_CRC           (1u << 4)
#define LEAP_FLAG_FRAGMENTED               (1u << 5)
#define LEAP_FLAG_TIME_VALID               (1u << 6)
#define LEAP_FLAG_RESERVED7                (1u << 7)

#define LEAP_FLAGS_RESERVED_MASK           LEAP_FLAG_RESERVED7

/* -------------------------------------------------------------------------- */
/* Service registry                                                           */
/* -------------------------------------------------------------------------- */

typedef enum LeapServiceId_u16
{
    LEAP_SERVICE_MGMT                      = 0x0001u,
    LEAP_SERVICE_DISC                      = 0x0002u,
    LEAP_SERVICE_DIR                       = 0x0003u,
    LEAP_SERVICE_PD                        = 0x0010u,
    LEAP_SERVICE_DIAG                      = 0x0020u,

    LEAP_SERVICE_VENDOR_FIRST              = 0x8000u,
    LEAP_SERVICE_VENDOR_LAST               = 0xFFFEu,

    LEAP_SERVICE_INVALID                   = 0xFFFFu
} LeapServiceId_u16;

/* -------------------------------------------------------------------------- */
/* Status codes                                                               */
/* -------------------------------------------------------------------------- */

typedef enum LeapStatusCode_u16
{
    LEAP_STATUS_OK                         = 0x0000u,
    LEAP_STATUS_UNSUPPORTED_VERSION        = 0x0001u,
    LEAP_STATUS_BAD_LENGTH                 = 0x0002u,
    LEAP_STATUS_BAD_CHECK                  = 0x0003u,
    LEAP_STATUS_UNSUPPORTED_SERVICE        = 0x0004u,
    LEAP_STATUS_UNSUPPORTED_MESSAGE        = 0x0005u,
    LEAP_STATUS_INVALID_STATE              = 0x0006u,
    LEAP_STATUS_NOT_OWNER                  = 0x0007u,
    LEAP_STATUS_LEASE_EXPIRED              = 0x0008u,
    LEAP_STATUS_BUSY                       = 0x0009u,
    LEAP_STATUS_FAULTED                    = 0x000Au,
    LEAP_STATUS_RANGE                      = 0x000Bu,
    LEAP_STATUS_RATE_LIMITED               = 0x000Cu,

    /* v1.0 switch-safe / commissioning additions */
    LEAP_STATUS_STALE_FRAME                = 0x000Du,
    LEAP_STATUS_OUT_OF_ORDER               = 0x000Eu,
    LEAP_STATUS_DUPLICATE_SEQUENCE         = 0x000Fu,
    LEAP_STATUS_PROFILE_MISMATCH           = 0x0010u,
    LEAP_STATUS_WATCHDOG_EXPIRED           = 0x0011u,
    LEAP_STATUS_LOCATE_UNAVAILABLE         = 0x0012u,
    LEAP_STATUS_AUTH_REQUIRED              = 0x0013u,
    LEAP_STATUS_PERMISSION_DENIED          = 0x0014u,

    LEAP_STATUS_VENDOR_FIRST               = 0x8000u
} LeapStatusCode_u16;

/* -------------------------------------------------------------------------- */
/* Device states                                                              */
/* -------------------------------------------------------------------------- */

typedef enum LeapState_u16
{
    LEAP_STATE_BOOT                        = 0x0000u,
    LEAP_STATE_INIT                        = 0x0001u,
    LEAP_STATE_CONFIGURED                  = 0x0002u,
    LEAP_STATE_SAFE                        = 0x0003u,
    LEAP_STATE_OP                          = 0x0004u,
    LEAP_STATE_FAULT                       = 0x0005u
} LeapState_u16;

/* -------------------------------------------------------------------------- */
/* Discovery messages                                                         */
/* -------------------------------------------------------------------------- */

typedef enum LeapDiscMessage_u16
{
    LEAP_DISC_HELLO                        = 0x0001u,
    LEAP_DISC_HELLO_REPLY                  = 0x0002u,
    LEAP_DISC_IDENTIFY                     = 0x0003u,
    LEAP_DISC_IDENTIFY_REPLY               = 0x0004u,
    LEAP_DISC_LOCATE_DEVICE                = 0x0005u,
    LEAP_DISC_LOCATE_DEVICE_REPLY          = 0x0006u
} LeapDiscMessage_u16;

/* -------------------------------------------------------------------------- */
/* Management messages                                                        */
/* -------------------------------------------------------------------------- */

typedef enum LeapMgmtMessage_u16
{
    LEAP_MGMT_OPEN_SESSION                 = 0x0001u,
    LEAP_MGMT_OPEN_SESSION_REPLY           = 0x0002u,
    LEAP_MGMT_CLOSE_SESSION                = 0x0003u,
    LEAP_MGMT_HEARTBEAT                    = 0x0004u,
    LEAP_MGMT_SET_STATE                    = 0x0005u,
    LEAP_MGMT_STATE_REPLY                  = 0x0006u,
    LEAP_MGMT_FAULT_RESET                  = 0x0007u,
    LEAP_MGMT_OWNER_RELEASE                = 0x0008u
} LeapMgmtMessage_u16;

/* -------------------------------------------------------------------------- */
/* Directory messages                                                         */
/* -------------------------------------------------------------------------- */

typedef enum LeapDirMessage_u16
{
    LEAP_DIR_READ_DIRECTORY                = 0x0001u,
    LEAP_DIR_READ_DIRECTORY_REPLY          = 0x0002u,
    LEAP_DIR_READ_OBJECT                   = 0x0003u,
    LEAP_DIR_READ_OBJECT_REPLY             = 0x0004u,
    LEAP_DIR_WRITE_OBJECT                  = 0x0005u,
    LEAP_DIR_WRITE_OBJECT_REPLY            = 0x0006u,
    LEAP_DIR_SELECT_PROFILE                = 0x0007u,
    LEAP_DIR_PROFILE_REPLY                 = 0x0008u
} LeapDirMessage_u16;

/* -------------------------------------------------------------------------- */
/* Process-data messages                                                      */
/* -------------------------------------------------------------------------- */

typedef enum LeapPdMessage_u16
{
    LEAP_PD_WRITE_ENDPOINT                 = 0x0001u,
    LEAP_PD_READ_ENDPOINT                  = 0x0002u,
    LEAP_PD_ENDPOINT_DATA                  = 0x0003u,
    LEAP_PD_EXCHANGE_ENDPOINTS             = 0x0004u,
    LEAP_PD_EXCHANGE_REPLY                 = 0x0005u
} LeapPdMessage_u16;

/* -------------------------------------------------------------------------- */
/* Diagnostics messages                                                       */
/* -------------------------------------------------------------------------- */

typedef enum LeapDiagMessage_u16
{
    LEAP_DIAG_READ_COUNTERS                = 0x0001u,
    LEAP_DIAG_COUNTERS_REPLY               = 0x0002u,
    LEAP_DIAG_READ_TIMING                  = 0x0003u,
    LEAP_DIAG_TIMING_REPLY                 = 0x0004u,
    LEAP_DIAG_READ_EVENTS                  = 0x0005u,
    LEAP_DIAG_EVENTS_REPLY                 = 0x0006u,
    LEAP_DIAG_TRACE_MARK                   = 0x0007u
} LeapDiagMessage_u16;

/* -------------------------------------------------------------------------- */
/* TLV registry                                                               */
/* -------------------------------------------------------------------------- */

typedef enum LeapTlvType_u16
{
    LEAP_TLV_PROTOCOL_VERSION_RANGE        = 0x0001u,
    LEAP_TLV_DEVICE_NAME                   = 0x0002u,
    LEAP_TLV_VENDOR_ID                     = 0x0003u,
    LEAP_TLV_PRODUCT_CODE                  = 0x0004u,
    LEAP_TLV_REVISION                      = 0x0005u,
    LEAP_TLV_SERIAL_NUMBER                 = 0x0006u,
    LEAP_TLV_PRIMARY_MAC                   = 0x0007u,
    LEAP_TLV_SUPPORTED_SERVICES            = 0x0008u,
    LEAP_TLV_DEFAULT_PROFILE_ID            = 0x0009u,
    LEAP_TLV_ACTIVE_PROFILE_ID             = 0x000Au,
    LEAP_TLV_CURRENT_STATE                 = 0x000Bu,
    LEAP_TLV_ACTIVE_OWNER_MAC              = 0x000Cu,
    LEAP_TLV_DEVICE_IDENTITY               = 0x000Du,
    LEAP_TLV_ENDPOINT_DESCRIPTOR           = 0x000Eu,
    LEAP_TLV_PROFILE_DESCRIPTOR            = 0x000Fu,
    LEAP_TLV_LOCATE_CAPABILITY             = 0x0010u,
    LEAP_TLV_SWITCH_SAFE_CAPABILITY        = 0x0011u,
    LEAP_TLV_MAX_FRAME_AGE_US              = 0x0012u,
    LEAP_TLV_PROFILE_NAME                  = 0x0013u,
    LEAP_TLV_SCHEMA_OBJECT_ID              = 0x0014u,

    LEAP_TLV_VENDOR_FIRST                  = 0x8000u
} LeapTlvType_u16;

/* -------------------------------------------------------------------------- */
/* Object ID namespaces                                                       */
/* -------------------------------------------------------------------------- */

#define LEAP_OBJECT_ID(namespace_u16, object_u16) \
    ((((uint32_t)(namespace_u16)) << 16) | ((uint32_t)(object_u16)))

typedef enum LeapObjectNamespace_u16
{
    LEAP_OBJ_NS_IDENTITY                   = 0x0001u,
    LEAP_OBJ_NS_MGMT_CONFIG                = 0x0002u,
    LEAP_OBJ_NS_ENDPOINT_PROFILE           = 0x0003u,
    LEAP_OBJ_NS_PERSISTENT_CONFIG          = 0x0004u,
    LEAP_OBJ_NS_DIAGNOSTICS                = 0x0005u,
    LEAP_OBJ_NS_VENDOR_FIRST               = 0x8000u,
    LEAP_OBJ_NS_VENDOR_LAST                = 0xFFFEu
} LeapObjectNamespace_u16;

/* -------------------------------------------------------------------------- */
/* Profile IDs (uint32 constants for strict C portability)                    */
/* -------------------------------------------------------------------------- */

typedef uint32_t LeapProfileId_u32;

#define LEAP_PROFILE_NONE                      0x00000000u

#define LEAP_PROFILE_DIGITAL_IO_8X8            0x00010001u
#define LEAP_PROFILE_DIGITAL_IO_16X16          0x00010002u
#define LEAP_PROFILE_DIGITAL_IO_32X32          0x00010003u

#define LEAP_PROFILE_ANALOG_IO_4AI             0x00020001u
#define LEAP_PROFILE_ANALOG_IO_4AO             0x00020002u
#define LEAP_PROFILE_ANALOG_IO_4AI_4AO         0x00020003u

#define LEAP_PROFILE_MIXED_IO_16DI_16DO_4AI_4AO 0x00030001u

#define LEAP_PROFILE_MOTION_SINGLE_AXIS        0x00040001u
#define LEAP_PROFILE_MOTION_MULTI_AXIS         0x00040002u

#define LEAP_PROFILE_VENDOR_FIRST              0x80000000u

/* -------------------------------------------------------------------------- */
/* Endpoint direction and flags                                               */
/* -------------------------------------------------------------------------- */

typedef enum LeapEndpointDirection_u8
{
    LEAP_ENDPOINT_DIR_INVALID              = 0u,
    LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE = 1u,
    LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER = 2u
} LeapEndpointDirection_u8;

#define LEAP_ENDPOINT_FLAG_FIXED           (1u << 0)
#define LEAP_ENDPOINT_FLAG_OPTIONAL        (1u << 1)
#define LEAP_ENDPOINT_FLAG_RETAINED        (1u << 2)
#define LEAP_ENDPOINT_FLAG_SAFE_STATE      (1u << 3)
#define LEAP_ENDPOINT_FLAG_FAULT_STATE     (1u << 4)
#define LEAP_ENDPOINT_FLAG_READABLE_SAFE   (1u << 5)
#define LEAP_ENDPOINT_FLAG_READABLE_FAULT  (1u << 6)
#define LEAP_ENDPOINT_FLAG_RESERVED7       (1u << 7)

/* Standard endpoint IDs. Profiles may define additional endpoints. */
#define LEAP_ENDPOINT_COMMAND              0x0001u
#define LEAP_ENDPOINT_STATUS               0x0002u
#define LEAP_ENDPOINT_DIGITAL_OUTPUTS      0x0010u
#define LEAP_ENDPOINT_DIGITAL_INPUTS       0x0011u
#define LEAP_ENDPOINT_ANALOG_OUTPUTS       0x0020u
#define LEAP_ENDPOINT_ANALOG_INPUTS        0x0021u
#define LEAP_ENDPOINT_SAFE_OUTPUTS         0x0030u
#define LEAP_ENDPOINT_FAULT_OUTPUTS        0x0031u
#define LEAP_ENDPOINT_VENDOR_FIRST         0x8000u

/* -------------------------------------------------------------------------- */
/* Locate-device patterns and flags                                           */
/* -------------------------------------------------------------------------- */

typedef enum LeapLocatePattern_u8
{
    LEAP_LOCATE_PATTERN_DEFAULT            = 0u,
    LEAP_LOCATE_PATTERN_SLOW_BLINK         = 1u,
    LEAP_LOCATE_PATTERN_FAST_BLINK         = 2u,
    LEAP_LOCATE_PATTERN_DOUBLE_BLINK       = 3u,
    LEAP_LOCATE_PATTERN_SOLID              = 4u,
    LEAP_LOCATE_PATTERN_CUSTOM             = 255u
} LeapLocatePattern_u8;

#define LEAP_LOCATE_FLAG_LED               (1u << 0)
#define LEAP_LOCATE_FLAG_BUZZER            (1u << 1)
#define LEAP_LOCATE_FLAG_DISPLAY           (1u << 2)
#define LEAP_LOCATE_FLAG_CANCEL            (1u << 7)

/* -------------------------------------------------------------------------- */
/* Session / ownership flags                                                  */
/* -------------------------------------------------------------------------- */

#define LEAP_OPEN_FLAG_REQUEST_OWNER       (1u << 0)
#define LEAP_OPEN_FLAG_OBSERVER_ONLY       (1u << 1)
#define LEAP_OPEN_FLAG_STEAL_EXPIRED       (1u << 2)
/* Same-source-MAC owner recovery from a stale pre-reboot owner session. */
#define LEAP_OPEN_FLAG_REBOOT_RECOVERY     (1u << 3)

#define LEAP_SESSION_FLAG_OWNER            (1u << 0)
#define LEAP_SESSION_FLAG_OBSERVER         (1u << 1)
#define LEAP_SESSION_FLAG_LEASE_ACTIVE     (1u << 2)

/* -------------------------------------------------------------------------- */
/* Process-data flags                                                         */
/* -------------------------------------------------------------------------- */

#define LEAP_PD_FLAG_APPLY_OUTPUTS         (1u << 0)
#define LEAP_PD_FLAG_READBACK_REQUIRED     (1u << 1)
#define LEAP_PD_FLAG_TIMESTAMP_VALID       (1u << 2)
#define LEAP_PD_FLAG_ALLOW_SKIP            (1u << 3)

/* -------------------------------------------------------------------------- */
/* Diagnostic counter IDs                                                     */
/* -------------------------------------------------------------------------- */

typedef enum LeapDiagCounterId_u16
{
    LEAP_COUNTER_RX_FRAMES_ACCEPTED        = 0x0001u,
    LEAP_COUNTER_RX_FRAMES_REJECTED        = 0x0002u,
    LEAP_COUNTER_TX_FRAMES_ACCEPTED        = 0x0003u,
    LEAP_COUNTER_TX_FRAMES_DROPPED         = 0x0004u,
    LEAP_COUNTER_CRC_FAILURES              = 0x0005u,
    LEAP_COUNTER_BAD_LENGTH_FAILURES       = 0x0006u,
    LEAP_COUNTER_UNSUPPORTED_MESSAGES      = 0x0007u,
    LEAP_COUNTER_DUPLICATE_SEQUENCES       = 0x0008u,
    LEAP_COUNTER_LEASE_EXPIRATIONS         = 0x0009u,
    LEAP_COUNTER_STATE_TRANSITION_REJECTS  = 0x000Au,
    LEAP_COUNTER_PROCESS_CYCLES_ACCEPTED   = 0x000Bu,
    LEAP_COUNTER_PROCESS_CYCLES_MISSED     = 0x000Cu,

    /* Switch-safe diagnostics */
    LEAP_COUNTER_STALE_PROCESS_FRAMES      = 0x0010u,
    LEAP_COUNTER_LATE_PROCESS_FRAMES       = 0x0011u,
    LEAP_COUNTER_OUT_OF_ORDER_FRAMES       = 0x0012u,
    LEAP_COUNTER_REPLY_TIMEOUTS            = 0x0013u,
    LEAP_COUNTER_MAX_REPLY_LATENCY_US      = 0x0014u,
    LEAP_COUNTER_LAST_REPLY_LATENCY_US     = 0x0015u,
    LEAP_COUNTER_SWITCH_CONGESTION_HINTS   = 0x0016u,

    LEAP_COUNTER_VENDOR_FIRST              = 0x8000u
} LeapDiagCounterId_u16;

/* -------------------------------------------------------------------------- */
/* Event IDs                                                                  */
/* -------------------------------------------------------------------------- */

typedef enum LeapEventId_u16
{
    LEAP_EVENT_BOOT                        = 0x0001u,
    LEAP_EVENT_SESSION_OPENED              = 0x0002u,
    LEAP_EVENT_SESSION_CLOSED              = 0x0003u,
    LEAP_EVENT_OWNER_ACQUIRED              = 0x0004u,
    LEAP_EVENT_OWNER_RELEASED              = 0x0005u,
    LEAP_EVENT_STATE_CHANGED               = 0x0006u,
    LEAP_EVENT_FAULT_ENTERED               = 0x0007u,
    LEAP_EVENT_FAULT_RESET                 = 0x0008u,
    LEAP_EVENT_LOCATE_STARTED              = 0x0009u,
    LEAP_EVENT_LOCATE_STOPPED              = 0x000Au,
    LEAP_EVENT_STALE_FRAME_REJECTED        = 0x000Bu,
    LEAP_EVENT_WATCHDOG_EXPIRED            = 0x000Cu,

    LEAP_EVENT_VENDOR_FIRST                = 0x8000u
} LeapEventId_u16;

/* -------------------------------------------------------------------------- */
/* Packed wire structures                                                     */
/* -------------------------------------------------------------------------- */

LEAP_PACKED_BEGIN

typedef struct LEAP_PACKED LeapHeader
{
    uint32_t magic;              /* LEAP_MAGIC_U32 */
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  header_length;      /* 32 for v1 */
    uint8_t  flags;

    uint16_t service_id;
    uint16_t message_type;

    uint32_t session_id;
    uint32_t sequence;
    uint32_t ack_sequence;

    uint16_t payload_length;
    uint16_t header_crc16;       /* CRC-16 over header with this field zeroed */
    uint32_t payload_crc32c;     /* CRC-32C over payload, or 0 when allowed */
} LeapHeader;

typedef struct LEAP_PACKED LeapTlvHeader
{
    uint16_t type;
    uint16_t length;
    /* followed by length bytes, then 0-3 pad bytes to 4-byte boundary */
} LeapTlvHeader;

typedef struct LEAP_PACKED LeapErrorPayload
{
    uint16_t status_code;
    uint16_t detail_code;
    uint32_t rejected_sequence;
    uint32_t affected_offset;
    uint32_t affected_length;
} LeapErrorPayload;

/* Fixed device identity object. Strings belong in TLVs. */
typedef struct LEAP_PACKED LeapIdentity
{
    uint8_t  primary_mac[6];
    uint16_t vendor_id;
    uint32_t product_code;
    uint32_t serial_number;
    uint16_t hardware_revision;
    uint16_t firmware_revision;
    uint32_t device_capability_flags;
} LeapIdentity;

typedef struct LEAP_PACKED LeapVersionRange
{
    uint8_t min_major;
    uint8_t min_minor;
    uint8_t max_major;
    uint8_t max_minor;
} LeapVersionRange;

/* Discovery */

typedef struct LEAP_PACKED LeapHelloRequest
{
    uint16_t controller_capability_flags;
    uint16_t reserved;
} LeapHelloRequest;

typedef struct LEAP_PACKED LeapHelloReply
{
    LeapIdentity identity;
    uint32_t default_profile_id;
    uint32_t active_profile_id;
    uint16_t current_state;
    uint16_t supported_service_count; /* followed by uint16_t service IDs */
    uint8_t  active_owner_mac[6];
    uint16_t locate_capability_flags;
} LeapHelloReply;

typedef struct LEAP_PACKED LeapIdentifyRequest
{
    uint8_t target_mac[6];       /* all zero means "the receiving device" */
    uint16_t request_flags;
} LeapIdentifyRequest;

typedef struct LEAP_PACKED LeapIdentifyReply
{
    LeapIdentity identity;
    uint32_t default_profile_id;
    uint32_t active_profile_id;
    uint16_t current_state;
    uint16_t supported_service_count; /* followed by uint16_t service IDs */
    uint8_t  active_owner_mac[6];
    uint16_t locate_capability_flags;
} LeapIdentifyReply;

typedef struct LEAP_PACKED LeapLocateDeviceRequest
{
    uint16_t duration_ms;
    uint8_t  pattern;
    uint8_t  flags;
} LeapLocateDeviceRequest;

typedef struct LEAP_PACKED LeapLocateDeviceReply
{
    uint8_t  supported;
    uint8_t  active;
    uint16_t remaining_ms;
} LeapLocateDeviceReply;

/* Management */

typedef struct LEAP_PACKED LeapOpenSessionRequest
{
    uint8_t  controller_mac[6];
    uint16_t open_flags;
    uint32_t requested_lease_time_us;
    uint32_t requested_watchdog_time_us;
    uint32_t controller_capability_flags;
} LeapOpenSessionRequest;

typedef struct LEAP_PACKED LeapOpenSessionReply
{
    uint32_t assigned_session_id;
    uint32_t granted_lease_time_us;
    uint32_t granted_watchdog_time_us;
    uint16_t session_flags;
    uint16_t current_state;
    uint8_t  owner_mac[6];
    uint16_t reserved;
} LeapOpenSessionReply;

typedef struct LEAP_PACKED LeapCloseSessionRequest
{
    uint32_t session_id;
    uint16_t close_flags;
    uint16_t reserved;
} LeapCloseSessionRequest;

typedef struct LEAP_PACKED LeapHeartbeatPayload
{
    uint32_t latest_process_sequence;
    uint32_t controller_time_us_low;
    uint32_t controller_time_us_high;
    uint16_t current_controller_state;
    uint16_t heartbeat_flags;
} LeapHeartbeatPayload;

typedef struct LEAP_PACKED LeapSetStateRequest
{
    uint16_t requested_state;
    uint16_t transition_flags;
} LeapSetStateRequest;

typedef struct LEAP_PACKED LeapStateReply
{
    uint16_t accepted_state;
    uint16_t current_state;
    uint32_t state_detail;
} LeapStateReply;

typedef struct LEAP_PACKED LeapFaultResetRequest
{
    uint32_t reset_flags;
    uint32_t fault_code_ack;
} LeapFaultResetRequest;

typedef struct LEAP_PACKED LeapOwnerReleaseRequest
{
    uint32_t release_flags;
    uint32_t requested_safe_profile_id;
} LeapOwnerReleaseRequest;

/* Directory */

typedef struct LEAP_PACKED LeapReadDirectoryRequest
{
    uint32_t directory_flags;
    uint32_t start_object_id;
    uint16_t max_bytes;
    uint16_t reserved;
} LeapReadDirectoryRequest;

typedef struct LEAP_PACKED LeapReadDirectoryReply
{
    uint32_t directory_flags;
    uint16_t total_bytes;
    uint16_t returned_bytes;
    /* followed by TLVs */
} LeapReadDirectoryReply;

typedef struct LEAP_PACKED LeapReadObjectRequest
{
    uint32_t object_id;
    uint32_t offset;
    uint32_t length;
} LeapReadObjectRequest;

typedef struct LEAP_PACKED LeapReadObjectReply
{
    uint32_t object_id;
    uint32_t offset;
    uint32_t length;
    uint32_t object_flags;
    /* followed by object bytes */
} LeapReadObjectReply;

typedef struct LEAP_PACKED LeapWriteObjectRequest
{
    uint32_t object_id;
    uint32_t offset;
    uint32_t length;
    uint32_t write_flags;
    /* followed by object bytes */
} LeapWriteObjectRequest;

typedef struct LEAP_PACKED LeapWriteObjectReply
{
    uint32_t object_id;
    uint32_t accepted_offset;
    uint32_t accepted_length;
    uint32_t write_result_flags;
} LeapWriteObjectReply;

typedef struct LEAP_PACKED LeapSelectProfileRequest
{
    uint32_t requested_profile_id;
    uint32_t profile_flags;
} LeapSelectProfileRequest;

typedef struct LEAP_PACKED LeapProfileReply
{
    uint32_t active_profile_id;
    uint16_t endpoint_count;
    uint16_t profile_flags;
    /* followed by LeapEndpointDescriptor entries or TLVs */
} LeapProfileReply;

typedef struct LEAP_PACKED LeapEndpointDescriptor
{
    uint16_t endpoint_id;
    uint8_t  direction;
    uint8_t  flags;
    uint32_t profile_id;
    uint16_t byte_length;
    uint8_t  alignment;
    uint8_t  reserved;
    uint32_t schema_object_id;
} LeapEndpointDescriptor;

typedef struct LEAP_PACKED LeapProfileDescriptor
{
    uint32_t profile_id;
    uint16_t profile_revision;
    uint16_t endpoint_count;
    uint32_t profile_flags;
    uint32_t schema_object_id;
} LeapProfileDescriptor;

/* Process Data */

/*
 * Generic single-endpoint access header. Used by WRITE_ENDPOINT,
 * READ_ENDPOINT, and ENDPOINT_DATA.
 */
typedef struct LEAP_PACKED LeapEndpointDataHeader
{
    uint16_t endpoint_id;
    uint16_t endpoint_offset;
    uint16_t data_length;
    uint16_t endpoint_flags;
    uint32_t process_sequence;
    uint32_t cycle_time_us;
    uint64_t controller_timestamp_us;
    uint32_t max_frame_age_us;
    uint32_t profile_id;
    /* followed by data bytes when applicable */
} LeapEndpointDataHeader;

/*
 * Switch-safe combined exchange.
 * Request layout:
 *   LeapExchangeHeader
 *   write_length bytes of command/output data
 *   read_length bytes of zero-filled reservation
 *
 * Reply layout:
 *   LeapExchangeHeader
 *   write_length bytes echo or accepted command/output data per profile
 *   read_length bytes of status/input data
 */
typedef struct LEAP_PACKED LeapExchangeHeader
{
    uint16_t write_endpoint_id;
    uint16_t read_endpoint_id;
    uint16_t write_length;
    uint16_t read_length;
    uint32_t process_sequence;
    uint32_t cycle_time_us;
    uint64_t controller_timestamp_us;
    uint32_t max_frame_age_us;
    uint32_t profile_id;
    uint16_t exchange_flags;
    uint16_t reserved;
} LeapExchangeHeader;

typedef struct LEAP_PACKED LeapExchangeStatus
{
    uint32_t latest_process_sequence_consumed;
    uint32_t device_process_sequence;
    uint32_t measured_cycle_time_us;
    uint32_t device_timestamp_us_low;
    uint32_t device_timestamp_us_high;
    uint16_t status_code;
    uint16_t endpoint_status_flags;
} LeapExchangeStatus;

/* Diagnostics */

typedef struct LEAP_PACKED LeapReadCountersRequest
{
    uint16_t first_counter_id;
    uint16_t counter_count;
    uint32_t read_flags;
} LeapReadCountersRequest;

typedef struct LEAP_PACKED LeapCounterEntry
{
    uint16_t counter_id;
    uint16_t counter_flags;
    uint64_t value;
} LeapCounterEntry;

typedef struct LEAP_PACKED LeapCountersReply
{
    uint16_t counter_count;
    uint16_t reserved;
    /* followed by LeapCounterEntry entries */
} LeapCountersReply;

typedef struct LEAP_PACKED LeapReadTimingRequest
{
    uint32_t timing_flags;
} LeapReadTimingRequest;

typedef struct LEAP_PACKED LeapTimingReply
{
    uint32_t last_cycle_time_us;
    uint32_t max_cycle_time_us;
    uint32_t min_cycle_time_us;
    uint32_t last_reply_latency_us;
    uint32_t max_reply_latency_us;
    uint32_t process_watchdog_remaining_us;
    uint32_t owner_lease_remaining_us;
} LeapTimingReply;

typedef struct LEAP_PACKED LeapReadEventsRequest
{
    uint32_t start_index;
    uint16_t max_events;
    uint16_t event_flags;
} LeapReadEventsRequest;

typedef struct LEAP_PACKED LeapEventEntry
{
    uint32_t event_index;
    uint64_t timestamp_us;
    uint16_t event_id;
    uint16_t event_severity;
    uint32_t detail0;
    uint32_t detail1;
} LeapEventEntry;

typedef struct LEAP_PACKED LeapEventsReply
{
    uint32_t next_index;
    uint16_t event_count;
    uint16_t reserved;
    /* followed by LeapEventEntry entries */
} LeapEventsReply;

typedef struct LEAP_PACKED LeapTraceMarkRequest
{
    uint32_t trace_id;
    uint32_t value0;
    uint32_t value1;
} LeapTraceMarkRequest;

/* Fragmentation */

typedef struct LEAP_PACKED LeapFragmentHeader
{
    uint32_t fragment_group_id;
    uint16_t fragment_index;
    uint16_t fragment_count;
    uint32_t total_length;
    uint32_t total_crc32c;
} LeapFragmentHeader;

/* Standard Digital I/O payloads */

typedef struct LEAP_PACKED LeapDigitalIo8
{
    uint8_t bits;
} LeapDigitalIo8;

typedef struct LEAP_PACKED LeapDigitalIo16
{
    uint16_t bits;
} LeapDigitalIo16;

typedef struct LEAP_PACKED LeapDigitalIo32
{
    uint32_t bits;
} LeapDigitalIo32;

typedef struct LEAP_PACKED LeapAnalogIo4x16
{
    int16_t ch0;
    int16_t ch1;
    int16_t ch2;
    int16_t ch3;
} LeapAnalogIo4x16;

/*
 * LEAP reference profile payload: DIGITAL_IO_16X16
 * Profile ID: LEAP_PROFILE_DIGITAL_IO_16X16 (0x00010002)
 * Total size: 8 bytes.
 */
typedef struct LEAP_PACKED LeapProfileDigital16x16
{
    uint16_t digital_inputs;    /* bits 0..15 => digital input channels 1..16 */
    uint16_t digital_outputs;   /* bits 0..15 => digital output channels 1..16 */
    uint16_t io_status;         /* module status flags */
    uint8_t  v_field_supply;    /* field supply voltage in 0.1V units */
    uint8_t  reserved0;         /* reserved; must be zero */
} LeapProfileDigital16x16;

/* Status flags for LeapProfileDigital16x16.io_status */
#define LEAP_DIO_STATUS_OK                 0x0000u
#define LEAP_DIO_STATUS_FIELD_POWER_FAULT  0x0001u
#define LEAP_DIO_STATUS_OUTPUT_SHORT       0x0002u
#define LEAP_DIO_STATUS_WIRE_BREAK         0x0004u
#define LEAP_DIO_STATUS_THERMAL_WARN       0x0008u

/* Optional aliases used by external documents/tools. */
#define LEAP_DIO_STATUS_NORMAL             LEAP_DIO_STATUS_OK
#define LEAP_DIO_STATUS_FIELD_PWR_FAULT    LEAP_DIO_STATUS_FIELD_POWER_FAULT

LEAP_PACKED_END

/* -------------------------------------------------------------------------- */
/* Size checks                                                                */
/* -------------------------------------------------------------------------- */

LEAP_STATIC_ASSERT(sizeof(LeapHeader) == 32, "LeapHeader must be 32 bytes");
LEAP_STATIC_ASSERT(sizeof(LeapTlvHeader) == 4, "LeapTlvHeader must be 4 bytes");
LEAP_STATIC_ASSERT(sizeof(LeapErrorPayload) == 16, "LeapErrorPayload must be 16 bytes");
LEAP_STATIC_ASSERT(sizeof(LeapIdentity) == 24, "LeapIdentity must be 24 bytes");
LEAP_STATIC_ASSERT(sizeof(LeapVersionRange) == 4, "LeapVersionRange must be 4 bytes");

LEAP_STATIC_ASSERT(sizeof(LeapHelloRequest) == 4, "LeapHelloRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapHelloReply) == 44, "LeapHelloReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapIdentifyRequest) == 8, "LeapIdentifyRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapIdentifyReply) == 44, "LeapIdentifyReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapLocateDeviceRequest) == 4, "LeapLocateDeviceRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapLocateDeviceReply) == 4, "LeapLocateDeviceReply size mismatch");

LEAP_STATIC_ASSERT(sizeof(LeapOpenSessionRequest) == 20, "LeapOpenSessionRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapOpenSessionReply) == 24, "LeapOpenSessionReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapCloseSessionRequest) == 8, "LeapCloseSessionRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapHeartbeatPayload) == 16, "LeapHeartbeatPayload size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapSetStateRequest) == 4, "LeapSetStateRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapStateReply) == 8, "LeapStateReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapFaultResetRequest) == 8, "LeapFaultResetRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapOwnerReleaseRequest) == 8, "LeapOwnerReleaseRequest size mismatch");

LEAP_STATIC_ASSERT(sizeof(LeapReadDirectoryRequest) == 12, "LeapReadDirectoryRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapReadDirectoryReply) == 8, "LeapReadDirectoryReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapReadObjectRequest) == 12, "LeapReadObjectRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapReadObjectReply) == 16, "LeapReadObjectReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapWriteObjectRequest) == 16, "LeapWriteObjectRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapWriteObjectReply) == 16, "LeapWriteObjectReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapSelectProfileRequest) == 8, "LeapSelectProfileRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapProfileReply) == 8, "LeapProfileReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapEndpointDescriptor) == 16, "LeapEndpointDescriptor size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapProfileDescriptor) == 16, "LeapProfileDescriptor size mismatch");

LEAP_STATIC_ASSERT(sizeof(LeapEndpointDataHeader) == 32, "LeapEndpointDataHeader size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapExchangeHeader) == 36, "LeapExchangeHeader size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapExchangeStatus) == 24, "LeapExchangeStatus size mismatch");

LEAP_STATIC_ASSERT(sizeof(LeapReadCountersRequest) == 8, "LeapReadCountersRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapCounterEntry) == 12, "LeapCounterEntry size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapCountersReply) == 4, "LeapCountersReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapReadTimingRequest) == 4, "LeapReadTimingRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapTimingReply) == 28, "LeapTimingReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapReadEventsRequest) == 8, "LeapReadEventsRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapEventEntry) == 24, "LeapEventEntry size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapEventsReply) == 8, "LeapEventsReply size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapTraceMarkRequest) == 12, "LeapTraceMarkRequest size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapFragmentHeader) == 16, "LeapFragmentHeader size mismatch");

LEAP_STATIC_ASSERT(sizeof(LeapDigitalIo8) == 1, "LeapDigitalIo8 size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapDigitalIo16) == 2, "LeapDigitalIo16 size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapDigitalIo32) == 4, "LeapDigitalIo32 size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapAnalogIo4x16) == 8, "LeapAnalogIo4x16 size mismatch");
LEAP_STATIC_ASSERT(sizeof(LeapProfileDigital16x16) == 8, "LeapProfileDigital16x16 size mismatch");

/* -------------------------------------------------------------------------- */
/* Small helpers safe for C/C++ headers                                       */
/* -------------------------------------------------------------------------- */

#define LEAP_TLV_PADDED_LENGTH(length_u16) \
    ((uint16_t)((((uint16_t)(length_u16)) + 3u) & 0xFFFCu))

#define LEAP_TLV_TOTAL_LENGTH(length_u16) \
    ((uint16_t)(sizeof(LeapTlvHeader) + LEAP_TLV_PADDED_LENGTH(length_u16)))

#define LEAP_IS_VENDOR_SERVICE(service_id_u16) \
    (((uint16_t)(service_id_u16) >= LEAP_SERVICE_VENDOR_FIRST) && ((uint16_t)(service_id_u16) <= LEAP_SERVICE_VENDOR_LAST))

#define LEAP_IS_VENDOR_TLV(tlv_type_u16) \
    ((uint16_t)(tlv_type_u16) >= LEAP_TLV_VENDOR_FIRST)

#define LEAP_IS_VENDOR_PROFILE(profile_id_u32) \
    ((uint32_t)(profile_id_u32) >= LEAP_PROFILE_VENDOR_FIRST)

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PROTOCOL_H */
