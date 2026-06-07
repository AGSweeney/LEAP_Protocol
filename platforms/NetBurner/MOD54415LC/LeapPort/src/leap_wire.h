#pragma once

#include <stdint.h>

struct __attribute__((packed)) LeapHeaderWire
{
    uint32_t magic;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t header_length;
    uint8_t flags;
    uint16_t service_id;
    uint16_t message_type;
    uint32_t session_id;
    uint32_t sequence;
    uint32_t ack_sequence;
    uint16_t payload_length;
    uint16_t header_crc16;
    uint32_t payload_crc32c;
};

struct __attribute__((packed)) LeapIdentityWire
{
    uint8_t primary_mac[6];
    uint16_t vendor_id;
    uint32_t product_code;
    uint32_t serial_number;
    uint16_t hardware_revision;
    uint16_t firmware_revision;
    uint32_t device_capability_flags;
};

struct __attribute__((packed)) LeapHelloReplyWire
{
    LeapIdentityWire identity;
    uint32_t default_profile_id;
    uint32_t active_profile_id;
    uint16_t current_state;
    uint16_t supported_service_count;
    uint8_t active_owner_mac[6];
    uint16_t locate_capability_flags;
};

struct __attribute__((packed)) LeapLocateDeviceRequestWire
{
    uint16_t duration_ms;
    uint8_t pattern;
    uint8_t flags;
};

struct __attribute__((packed)) LeapLocateDeviceReplyWire
{
    uint8_t supported;
    uint8_t active;
    uint16_t remaining_ms;
};

struct __attribute__((packed)) LeapOpenSessionRequestWire
{
    uint8_t controller_mac[6];
    uint16_t open_flags;
    uint32_t requested_lease_time_us;
    uint32_t requested_watchdog_time_us;
    uint32_t controller_capability_flags;
};

struct __attribute__((packed)) LeapOpenSessionReplyWire
{
    uint32_t assigned_session_id;
    uint32_t granted_lease_time_us;
    uint32_t granted_watchdog_time_us;
    uint16_t session_flags;
    uint16_t current_state;
    uint8_t owner_mac[6];
    uint16_t reserved;
};

struct __attribute__((packed)) LeapSetStateRequestWire
{
    uint16_t requested_state;
    uint16_t transition_flags;
};

struct __attribute__((packed)) LeapStateReplyWire
{
    uint16_t accepted_state;
    uint16_t current_state;
    uint32_t state_detail;
};

struct __attribute__((packed)) LeapTlvHeaderWire
{
    uint16_t type;
    uint16_t length;
};

struct __attribute__((packed)) LeapReadDirectoryReplyWire
{
    uint32_t directory_flags;
    uint16_t total_bytes;
    uint16_t returned_bytes;
};

struct __attribute__((packed)) LeapReadObjectRequestWire
{
    uint32_t object_id;
    uint32_t offset;
    uint32_t length;
};

struct __attribute__((packed)) LeapReadObjectReplyWire
{
    uint32_t object_id;
    uint32_t offset;
    uint32_t length;
    uint32_t object_flags;
};

struct __attribute__((packed)) LeapSelectProfileRequestWire
{
    uint32_t requested_profile_id;
    uint32_t profile_flags;
};

struct __attribute__((packed)) LeapProfileReplyWire
{
    uint32_t active_profile_id;
    uint16_t endpoint_count;
    uint16_t profile_flags;
};

struct __attribute__((packed)) LeapEndpointDescriptorWire
{
    uint16_t endpoint_id;
    uint8_t direction;
    uint8_t flags;
    uint32_t profile_id;
    uint16_t byte_length;
    uint8_t alignment;
    uint8_t reserved;
    uint32_t schema_object_id;
};

struct __attribute__((packed)) LeapProfileDescriptorWire
{
    uint32_t profile_id;
    uint16_t profile_revision;
    uint16_t endpoint_count;
    uint32_t profile_flags;
    uint32_t schema_object_id;
};

struct __attribute__((packed)) LeapEndpointDataHeaderWire
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
};

struct __attribute__((packed)) LeapExchangeHeaderWire
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
};

struct __attribute__((packed)) LeapExchangeStatusWire
{
    uint32_t latest_process_sequence_consumed;
    uint32_t device_process_sequence;
    uint32_t measured_cycle_time_us;
    uint32_t device_timestamp_us_low;
    uint32_t device_timestamp_us_high;
    uint16_t status_code;
    uint16_t endpoint_status_flags;
};

struct __attribute__((packed)) LeapCountersReplyWire
{
    uint16_t counter_count;
    uint16_t reserved;
};

struct __attribute__((packed)) LeapCounterEntryWire
{
    uint16_t counter_id;
    uint16_t counter_flags;
    uint64_t value;
};

struct __attribute__((packed)) LeapTimingReplyWire
{
    uint32_t last_cycle_time_us;
    uint32_t max_cycle_time_us;
    uint32_t min_cycle_time_us;
    uint32_t last_reply_latency_us;
    uint32_t max_reply_latency_us;
    uint32_t process_watchdog_remaining_us;
    uint32_t owner_lease_remaining_us;
};
