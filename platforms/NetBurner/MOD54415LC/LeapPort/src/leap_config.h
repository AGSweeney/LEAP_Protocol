#pragma once

#include <stdint.h>

// LEAP v1 development transport defaults.
static constexpr uint16_t LEAP_ETHERTYPE_DEV = 0x88B6;
static constexpr uint16_t LEAP_ETHERTYPE_ALT = 0x88B5;

// Ethernet frame sizing constraints from LEAP transport spec.
static constexpr uint16_t LEAP_HEADER_BYTES = 32U;
static constexpr uint16_t LEAP_MAX_PAYLOAD_BYTES = 1468U;
static constexpr uint16_t LEAP_MAX_FRAME_BYTES = LEAP_HEADER_BYTES + LEAP_MAX_PAYLOAD_BYTES;

// LEAP requires minimum transmitted Ethernet payload of 50 bytes.
static constexpr uint16_t LEAP_MIN_TX_ETH_PAYLOAD = 50U;

// Default EtherType for this port bring-up.
static constexpr uint16_t LEAP_ETHERTYPE_IN_USE = LEAP_ETHERTYPE_DEV;

// LEAP v1 header constants.
static constexpr uint32_t LEAP_MAGIC_U32 = 0x5041454CUL;
static constexpr uint8_t LEAP_VERSION_MAJOR = 1U;
static constexpr uint8_t LEAP_VERSION_MINOR = 0U;
static constexpr uint8_t LEAP_FLAG_RESPONSE = (1U << 1);
static constexpr uint8_t LEAP_FLAG_NO_PAYLOAD_CRC = (1U << 4);

// LEAP core service/message IDs used by discovery scaffold.
static constexpr uint16_t LEAP_SERVICE_DISC = 0x0002U;
static constexpr uint16_t LEAP_DISC_HELLO = 0x0001U;
static constexpr uint16_t LEAP_DISC_HELLO_REPLY = 0x0002U;
static constexpr uint16_t LEAP_DISC_IDENTIFY = 0x0003U;
static constexpr uint16_t LEAP_DISC_IDENTIFY_REPLY = 0x0004U;
static constexpr uint16_t LEAP_DISC_LOCATE_DEVICE = 0x0005U;
static constexpr uint16_t LEAP_DISC_LOCATE_DEVICE_REPLY = 0x0006U;

static constexpr uint16_t LEAP_SERVICE_MGMT = 0x0001U;
static constexpr uint16_t LEAP_SERVICE_DIR = 0x0003U;
static constexpr uint16_t LEAP_SERVICE_PD = 0x0010U;
static constexpr uint16_t LEAP_SERVICE_DIAG = 0x0020U;

static constexpr uint16_t LEAP_MGMT_OPEN_SESSION = 0x0001U;
static constexpr uint16_t LEAP_MGMT_OPEN_SESSION_REPLY = 0x0002U;
static constexpr uint16_t LEAP_MGMT_CLOSE_SESSION = 0x0003U;
static constexpr uint16_t LEAP_MGMT_HEARTBEAT = 0x0004U;
static constexpr uint16_t LEAP_MGMT_SET_STATE = 0x0005U;
static constexpr uint16_t LEAP_MGMT_STATE_REPLY = 0x0006U;
static constexpr uint16_t LEAP_MGMT_OWNER_RELEASE = 0x0008U;

static constexpr uint16_t LEAP_DIR_READ_DIRECTORY = 0x0001U;
static constexpr uint16_t LEAP_DIR_READ_DIRECTORY_REPLY = 0x0002U;
static constexpr uint16_t LEAP_DIR_READ_OBJECT = 0x0003U;
static constexpr uint16_t LEAP_DIR_READ_OBJECT_REPLY = 0x0004U;
static constexpr uint16_t LEAP_DIR_SELECT_PROFILE = 0x0007U;
static constexpr uint16_t LEAP_DIR_PROFILE_REPLY = 0x0008U;

static constexpr uint16_t LEAP_PD_WRITE_ENDPOINT = 0x0001U;
static constexpr uint16_t LEAP_PD_READ_ENDPOINT = 0x0002U;
static constexpr uint16_t LEAP_PD_ENDPOINT_DATA = 0x0003U;
static constexpr uint16_t LEAP_PD_EXCHANGE_ENDPOINTS = 0x0004U;
static constexpr uint16_t LEAP_PD_EXCHANGE_REPLY = 0x0005U;

static constexpr uint16_t LEAP_DIAG_READ_COUNTERS = 0x0001U;
static constexpr uint16_t LEAP_DIAG_COUNTERS_REPLY = 0x0002U;
static constexpr uint16_t LEAP_DIAG_READ_TIMING = 0x0003U;
static constexpr uint16_t LEAP_DIAG_TIMING_REPLY = 0x0004U;

static constexpr uint16_t LEAP_STATE_CONFIGURED = 0x0002U;
static constexpr uint16_t LEAP_STATE_SAFE = 0x0003U;
static constexpr uint16_t LEAP_STATE_OP = 0x0004U;
static constexpr uint16_t LEAP_LOCATE_FLAG_LED = 0x0001U;

static constexpr uint16_t LEAP_OPEN_FLAG_REQUEST_OWNER = 0x0001U;
static constexpr uint16_t LEAP_SESSION_FLAG_OWNER = 0x0001U;
static constexpr uint16_t LEAP_SESSION_FLAG_LEASE_ACTIVE = 0x0004U;

static constexpr uint16_t LEAP_TLV_DEVICE_IDENTITY = 0x000DU;
static constexpr uint16_t LEAP_TLV_DEFAULT_PROFILE_ID = 0x0009U;
static constexpr uint16_t LEAP_TLV_ACTIVE_PROFILE_ID = 0x000AU;
static constexpr uint16_t LEAP_TLV_PROFILE_DESCRIPTOR = 0x000FU;
static constexpr uint16_t LEAP_TLV_ENDPOINT_DESCRIPTOR = 0x000EU;
static constexpr uint16_t LEAP_TLV_LOCATE_CAPABILITY = 0x0010U;

static constexpr uint16_t LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE = 1U;
static constexpr uint16_t LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER = 2U;
static constexpr uint16_t LEAP_ENDPOINT_DIGITAL_OUTPUTS = 0x0010U;
static constexpr uint16_t LEAP_ENDPOINT_DIGITAL_INPUTS = 0x0011U;

static constexpr uint32_t LEAP_PROFILE_DIGITAL_IO_8X8 = 0x00010001UL;
static constexpr uint32_t LEAP_DIR_IDENTITY_OBJECT_ID = 0x00010001UL;
static constexpr uint32_t LEAP_DIR_PROFILE_OBJECT_ID = 0x00030001UL;
static constexpr uint16_t LEAP_DIO_STATUS_OK = 0x0000U;
static constexpr uint16_t LEAP_STATUS_OK = 0x0000U;

static constexpr uint16_t LEAP_COUNTER_RX_FRAMES_ACCEPTED = 0x0001U;
static constexpr uint16_t LEAP_COUNTER_RX_FRAMES_REJECTED = 0x0002U;
static constexpr uint16_t LEAP_COUNTER_TX_FRAMES_ACCEPTED = 0x0003U;
static constexpr uint16_t LEAP_COUNTER_TX_FRAMES_DROPPED = 0x0004U;
static constexpr uint16_t LEAP_COUNTER_CRC_FAILURES = 0x0005U;
static constexpr uint16_t LEAP_COUNTER_BAD_LENGTH_FAILURES = 0x0006U;
static constexpr uint16_t LEAP_COUNTER_UNSUPPORTED_MESSAGES = 0x0007U;
static constexpr uint16_t LEAP_COUNTER_DUPLICATE_SEQUENCES = 0x0008U;
static constexpr uint16_t LEAP_COUNTER_LEASE_EXPIRATIONS = 0x0009U;
static constexpr uint16_t LEAP_COUNTER_STATE_TRANSITION_REJECTS = 0x000AU;
static constexpr uint16_t LEAP_COUNTER_PROCESS_CYCLES_ACCEPTED = 0x000BU;
static constexpr uint16_t LEAP_COUNTER_PROCESS_CYCLES_MISSED = 0x000CU;
static constexpr uint16_t LEAP_COUNTER_STALE_PROCESS_FRAMES = 0x0010U;
static constexpr uint16_t LEAP_COUNTER_LATE_PROCESS_FRAMES = 0x0011U;
static constexpr uint16_t LEAP_COUNTER_OUT_OF_ORDER_FRAMES = 0x0012U;
static constexpr uint16_t LEAP_COUNTER_REPLY_TIMEOUTS = 0x0013U;
static constexpr uint16_t LEAP_COUNTER_MAX_REPLY_LATENCY_US = 0x0014U;
static constexpr uint16_t LEAP_COUNTER_LAST_REPLY_LATENCY_US = 0x0015U;
static constexpr uint16_t LEAP_COUNTER_SWITCH_CONGESTION_HINTS = 0x0016U;

static constexpr uint16_t LEAP_HEADER_CRC_OFFSET = 26U;
static constexpr uint16_t LEAP_ETH_HEADER_BYTES = 14U;
