-- LEAP - Lightweight Ethernet Application Protocol Wireshark Dissector (initial)
-- Draft v1.0 wire contract (LeapHeader is 32 bytes, little-endian fields).

local leap_proto = Proto("LEAP", "Lightweight Ethernet Application Protocol")

-- Header fields
local f_magic         = ProtoField.string("leap.magic", "Magic")
local f_ver_major     = ProtoField.uint8("leap.version_major", "Version Major", BASE_DEC)
local f_ver_minor     = ProtoField.uint8("leap.version_minor", "Version Minor", BASE_DEC)
local f_header_len    = ProtoField.uint8("leap.header_length", "Header Length", BASE_DEC)
local f_flags         = ProtoField.uint8("leap.flags", "Flags", BASE_HEX)
local f_service_id    = ProtoField.uint16("leap.service_id", "Service ID", BASE_HEX)
local f_message_type  = ProtoField.uint16("leap.message_type", "Message Type", BASE_HEX)
local f_session_id    = ProtoField.uint32("leap.session_id", "Session ID", BASE_HEX)
local f_sequence      = ProtoField.uint32("leap.sequence", "Sequence", BASE_DEC)
local f_ack_sequence  = ProtoField.uint32("leap.ack_sequence", "ACK Sequence", BASE_DEC)
local f_payload_len   = ProtoField.uint16("leap.payload_length", "Payload Length", BASE_DEC)
local f_header_crc16  = ProtoField.uint16("leap.header_crc16", "Header CRC-16/XMODEM", BASE_HEX)
local f_payload_crc32 = ProtoField.uint32("leap.payload_crc32c", "Payload CRC-32C", BASE_HEX)

-- LeapFragmentHeader fields (present when LEAP_FLAG_FRAGMENTED is set, offset 32 in payload)
-- Wire layout: fragment_group_id(4) | fragment_index(2) | fragment_count(2) |
--              total_length(4) | total_crc32c(4)  — 16 bytes, no padding
local f_frag_group_id    = ProtoField.uint32("leap.frag.group_id",     "Fragment Group ID",   BASE_HEX)
local f_frag_index       = ProtoField.uint16("leap.frag.index",        "Fragment Index",      BASE_DEC)
local f_frag_count       = ProtoField.uint16("leap.frag.count",        "Fragment Count",      BASE_DEC)
local f_frag_total_len   = ProtoField.uint32("leap.frag.total_length", "Total Length",        BASE_DEC)
local f_frag_total_crc   = ProtoField.uint32("leap.frag.total_crc32c", "Total CRC-32C",       BASE_HEX)

-- LEAP-PD LeapEndpointDataHeader fields
local f_ep_id         = ProtoField.uint16("leap.pd.endpoint_id", "Endpoint ID", BASE_HEX)
local f_ep_offset     = ProtoField.uint16("leap.pd.endpoint_offset", "Endpoint Offset", BASE_DEC)
local f_data_len      = ProtoField.uint16("leap.pd.data_length", "Data Length", BASE_DEC)
local f_ep_flags      = ProtoField.uint16("leap.pd.endpoint_flags", "Endpoint Flags", BASE_HEX)
local f_proc_seq      = ProtoField.uint32("leap.pd.process_sequence", "Process Sequence", BASE_DEC)
local f_cycle_us      = ProtoField.uint32("leap.pd.cycle_time_us", "Cycle Time (us)", BASE_DEC)
local f_ts_us         = ProtoField.uint64("leap.pd.controller_timestamp_us", "Controller Timestamp (us)", BASE_DEC)
local f_max_age_us    = ProtoField.uint32("leap.pd.max_frame_age_us", "Max Frame Age (us)", BASE_DEC)
local f_profile_id    = ProtoField.uint32("leap.pd.profile_id", "Profile ID", BASE_HEX)

-- DIGITAL_IO_16X16 payload fields
local f_dio_inputs    = ProtoField.uint16("leap.dio16.inputs", "Digital Inputs (1-16)", BASE_HEX)
local f_dio_outputs   = ProtoField.uint16("leap.dio16.outputs", "Digital Outputs (1-16)", BASE_HEX)
local f_dio_status    = ProtoField.uint16("leap.dio16.status", "I/O Status", BASE_HEX)
local f_dio_vsupply   = ProtoField.uint8("leap.dio16.v_field_supply", "Field Supply (0.1V)", BASE_DEC)
local f_dio_reserved  = ProtoField.uint8("leap.dio16.reserved0", "Reserved", BASE_HEX)

local f_raw_payload   = ProtoField.bytes("leap.payload", "Payload")
local f_padding       = ProtoField.bytes("leap.padding", "Ethernet Padding")

leap_proto.fields = {
    f_magic, f_ver_major, f_ver_minor, f_header_len, f_flags, f_service_id, f_message_type,
    f_session_id, f_sequence, f_ack_sequence, f_payload_len, f_header_crc16, f_payload_crc32,
    f_frag_group_id, f_frag_index, f_frag_count, f_frag_total_len, f_frag_total_crc,
    f_ep_id, f_ep_offset, f_data_len, f_ep_flags, f_proc_seq, f_cycle_us, f_ts_us, f_max_age_us, f_profile_id,
    f_dio_inputs, f_dio_outputs, f_dio_status, f_dio_vsupply, f_dio_reserved,
    f_raw_payload, f_padding
}

local LEAP_HEADER_LEN                = 32
local LEAP_FRAGMENT_HEADER_LEN       = 16
local LEAP_EP_DATA_HEADER_LEN        = 32

-- Flag bits (byte 7 of LeapHeader)
local LEAP_FLAG_FRAGMENTED           = 0x20  -- bit 5

local LEAP_SERVICE_PD                = 0x0010
local LEAP_PD_WRITE_ENDPOINT         = 0x0001
local LEAP_PD_ENDPOINT_DATA          = 0x0003
local LEAP_PROFILE_DIGITAL_IO_16X16  = 0x00010002

function leap_proto.dissector(buffer, pinfo, tree)
    -- Use reported (captured) length for all bounds decisions.
    local length = buffer:reported_length_remaining()
    if length < LEAP_HEADER_LEN then return end

    -- Validate magic before claiming this frame.
    if buffer(0, 4):string() ~= "LEAP" then return end

    pinfo.cols.protocol = "LEAP"

    local subtree = tree:add(leap_proto, buffer(0, length), "LEAP Protocol Data")
    subtree:add(f_magic,        buffer(0,  4))
    subtree:add(f_ver_major,    buffer(4,  1))
    subtree:add(f_ver_minor,    buffer(5,  1))
    subtree:add(f_header_len,   buffer(6,  1))
    subtree:add(f_flags,        buffer(7,  1))
    subtree:add_le(f_service_id,    buffer(8,  2))
    subtree:add_le(f_message_type,  buffer(10, 2))
    subtree:add_le(f_session_id,    buffer(12, 4))
    subtree:add_le(f_sequence,      buffer(16, 4))
    subtree:add_le(f_ack_sequence,  buffer(20, 4))
    subtree:add_le(f_payload_len,   buffer(24, 2))
    subtree:add_le(f_header_crc16,  buffer(26, 2))
    subtree:add_le(f_payload_crc32, buffer(28, 4))

    local flags        = buffer(7,  1):uint()
    local service_id   = buffer(8,  2):le_uint()
    local message_type = buffer(10, 2):le_uint()
    local payload_len  = buffer(24, 2):le_uint()
    local is_fragmented = bit.band(flags, LEAP_FLAG_FRAGMENTED) ~= 0

    pinfo.cols.info = string.format(
        "Svc=0x%04X Msg=0x%04X Seq=%u Len=%u",
        service_id, message_type, buffer(16, 4):le_uint(), payload_len
    )

    local payload_offset = LEAP_HEADER_LEN

    -- Guard: payload_length must fit within the captured buffer.
    if payload_len == 0 then
        if length > payload_offset then
            subtree:add(f_padding, buffer(payload_offset, length - payload_offset))
        end
        return
    end

    if payload_len > (length - payload_offset) then
        subtree:add_expert_info(PI_MALFORMED, PI_ERROR,
            string.format("payload_length %u exceeds captured data (%u bytes available)",
                payload_len, length - payload_offset))
        return
    end

    local payload_tree = subtree:add(buffer(payload_offset, payload_len), "LEAP Payload")
    payload_tree:add(f_raw_payload, buffer(payload_offset, payload_len))

    -- Decode LeapFragmentHeader when FRAGMENTED flag (bit 5 = 0x20) is set.
    -- Layout: fragment_group_id(4) | fragment_index(2) | fragment_count(2) |
    --          total_length(4)     | total_crc32c(4)   — 16 bytes, no padding
    if is_fragmented then
        if payload_len < LEAP_FRAGMENT_HEADER_LEN then
            payload_tree:add_expert_info(PI_MALFORMED, PI_ERROR,
                "FRAGMENTED flag set but payload is too short for LeapFragmentHeader (need 16 bytes)")
            return
        end
        local frag_tree = payload_tree:add(
            buffer(payload_offset, LEAP_FRAGMENT_HEADER_LEN),
            "LEAP Fragment Header (LeapFragmentHeader)")
        frag_tree:add_le(f_frag_group_id,  buffer(payload_offset +  0, 4))
        frag_tree:add_le(f_frag_index,     buffer(payload_offset +  4, 2))
        frag_tree:add_le(f_frag_count,     buffer(payload_offset +  6, 2))
        frag_tree:add_le(f_frag_total_len, buffer(payload_offset +  8, 4))
        frag_tree:add_le(f_frag_total_crc, buffer(payload_offset + 12, 4))

        local frag_index = buffer(payload_offset + 4, 2):le_uint()
        local frag_count = buffer(payload_offset + 6, 2):le_uint()
        pinfo.cols.info = string.format(
            "Svc=0x%04X Msg=0x%04X Seq=%u [FRAG %u/%u]",
            service_id, message_type, buffer(16, 4):le_uint(),
            frag_index, frag_count)

        -- Advance past the fragment header. The remaining bytes are a raw
        -- fragment slice of a larger reassembled payload and are not decoded
        -- further by this dissector (full reassembly is out of scope here).
        payload_offset = payload_offset + LEAP_FRAGMENT_HEADER_LEN
        local remaining = payload_len - LEAP_FRAGMENT_HEADER_LEN
        if remaining > 0 then
            payload_tree:add(f_raw_payload, buffer(payload_offset, remaining)):set_text(
                string.format("Fragment Data (%u bytes, not decoded until reassembled)", remaining))
        end

        local consumed = LEAP_HEADER_LEN + payload_len
        if length > consumed then
            subtree:add(f_padding, buffer(consumed, length - consumed))
        end
        return
    end

    -- Decode LEAP-PD single-endpoint messages.
    if service_id == LEAP_SERVICE_PD
       and (message_type == LEAP_PD_WRITE_ENDPOINT or message_type == LEAP_PD_ENDPOINT_DATA)
       and payload_len >= LEAP_EP_DATA_HEADER_LEN then

        local pd = payload_tree:add(buffer(payload_offset, LEAP_EP_DATA_HEADER_LEN),
                                    "LEAP-PD Endpoint Header (LeapEndpointDataHeader)")
        pd:add_le(f_ep_id,      buffer(payload_offset +  0, 2))
        pd:add_le(f_ep_offset,  buffer(payload_offset +  2, 2))
        pd:add_le(f_data_len,   buffer(payload_offset +  4, 2))
        pd:add_le(f_ep_flags,   buffer(payload_offset +  6, 2))  -- endpoint_flags
        pd:add_le(f_proc_seq,   buffer(payload_offset +  8, 4))
        pd:add_le(f_cycle_us,   buffer(payload_offset + 12, 4))
        pd:add_le(f_ts_us,      buffer(payload_offset + 16, 8))
        pd:add_le(f_max_age_us, buffer(payload_offset + 24, 4))
        pd:add_le(f_profile_id, buffer(payload_offset + 28, 4))

        local profile_id     = buffer(payload_offset + 28, 4):le_uint()
        local pd_data_len    = buffer(payload_offset +  4, 2):le_uint()
        local pd_data_offset = payload_offset + LEAP_EP_DATA_HEADER_LEN

        -- Guard: profile data must fit within the declared payload.
        local available = payload_len - LEAP_EP_DATA_HEADER_LEN
        if available < pd_data_len then
            payload_tree:add_expert_info(PI_MALFORMED, PI_WARN,
                string.format("data_length %u exceeds available payload bytes %u",
                    pd_data_len, available))
        elseif profile_id == LEAP_PROFILE_DIGITAL_IO_16X16 and pd_data_len >= 8 and available >= 8 then
            local dio = payload_tree:add(buffer(pd_data_offset, 8),
                                         "Profile Payload: DIGITAL_IO_16X16")
            dio:add_le(f_dio_inputs,   buffer(pd_data_offset + 0, 2))
            dio:add_le(f_dio_outputs,  buffer(pd_data_offset + 2, 2))
            dio:add_le(f_dio_status,   buffer(pd_data_offset + 4, 2))
            dio:add(   f_dio_vsupply,  buffer(pd_data_offset + 6, 1))
            dio:add(   f_dio_reserved, buffer(pd_data_offset + 7, 1))
        end
    end

    local consumed = payload_offset + payload_len
    if length > consumed then
        subtree:add(f_padding, buffer(consumed, length - consumed))
    end
end

local eth_table = DissectorTable.get("ethertype")
eth_table:add(0x88B5, leap_proto)
eth_table:add(0x88B6, leap_proto)
