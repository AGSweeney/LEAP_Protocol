/*

 * LEAP Gateway (NetBurner MOD5441X) - mapping config backed by g_gateway (LeapOS-Gateway).

 *

 * SPDX-License-Identifier: MIT

 */



#ifdef LEAPGATEWAY_MAIN_TU



#include <cctype>

#include <cstdio>

#include <cstring>



extern "C" {

#include "gateway_global.h"

#include "gateway_leap_session.h"

#include "gateway_storage.h"

#include "leap/leap_gateway_config.h"

}



#define GW_MAX_MAPPINGS          LEAP_EIP_BRIDGE_MAX_MAPPINGS

#define GW_DEFAULT_PROFILE_ID    0x00010001u

#define GW_INPUT_ASSEMBLY_ID     100u

#define GW_OUTPUT_ASSEMBLY_ID    150u

#define GW_ASSEMBLY_BYTES        32u



static void GwMappingResetSlot(LeapEipBridgeMapping &slot, unsigned index)

{

    memset(&slot, 0, sizeof(slot));

    slot.profile_id = GW_DEFAULT_PROFILE_ID;

    slot.input.assembly_byte = static_cast<uint16_t>(index);

    slot.input.bit = 0u;

    slot.input.width_bits = 8u;

    slot.output.assembly_byte = static_cast<uint16_t>(index + 2u);

    slot.output.bit = 0u;

    slot.output.width_bits = 8u;

    slot.status_assembly_byte = static_cast<uint16_t>(index + 4u);

    slot.status_width_bytes = 1u;

    slot.enabled = 0;

}



static bool GwMappingMacIsZero(const uint8_t mac[6])

{

    if (!mac)

    {

        return true;

    }

    for (int i = 0; i < 6; ++i)

    {

        if (mac[i] != 0u)

        {

            return false;

        }

    }

    return true;

}



static bool GwMappingParseMacText(const char *text, uint8_t macOut[6])

{

    if (!text || !macOut)

    {

        return false;

    }



    unsigned values[6]{0};

    int matched = sscanf(text, "%02x:%02x:%02x:%02x:%02x:%02x",

                         &values[0], &values[1], &values[2],

                         &values[3], &values[4], &values[5]);

    if (matched != 6)

    {

        matched = sscanf(text, "%02x-%02x-%02x-%02x-%02x-%02x",

                         &values[0], &values[1], &values[2],

                         &values[3], &values[4], &values[5]);

    }

    if (matched != 6)

    {

        return false;

    }



    for (int i = 0; i < 6; ++i)

    {

        if (values[i] > 255u)

        {

            return false;

        }

        macOut[i] = static_cast<uint8_t>(values[i]);

    }

    return true;

}



static bool GwMappingParseProfileText(const char *text, uint32_t &profileOut)

{

    if (!text || !text[0])

    {

        return false;

    }



    char *end = nullptr;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))

    {

        profileOut = static_cast<uint32_t>(strtoul(text + 2, &end, 16));

    }

    else

    {

        profileOut = static_cast<uint32_t>(strtoul(text, &end, 16));

    }

    return end != text;

}



static void GwMappingWriteMacJson(int sock, const uint8_t mac[6])

{

    fdprintf(sock, "\"%02x:%02x:%02x:%02x:%02x:%02x\"",

             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

}



static unsigned GwMappingActiveCount()

{

    unsigned active = 0u;

    for (unsigned i = 0; i < g_gateway.config.bridge.mapping_count; ++i)

    {

        const LeapEipBridgeMapping &slot = g_gateway.config.bridge.mappings[i];

        if (slot.enabled && !GwMappingMacIsZero(slot.leap_mac))

        {

            ++active;

        }

    }

    return active;

}



static bool GwMappingValidateSlot(const LeapEipBridgeMapping &slot, const char **errorCode)

{

    *errorCode = nullptr;

    if (!slot.enabled)

    {

        return true;

    }

    if (GwMappingMacIsZero(slot.leap_mac))

    {

        *errorCode = "missing_mac";

        return false;

    }

    if (slot.input.assembly_byte >= GW_ASSEMBLY_BYTES ||

        slot.output.assembly_byte >= GW_ASSEMBLY_BYTES ||

        slot.status_assembly_byte >= GW_ASSEMBLY_BYTES)

    {

        *errorCode = "invalid_assembly_byte";

        return false;

    }

    if (slot.input.width_bits < 1u || slot.input.width_bits > 16u ||

        slot.output.width_bits < 1u || slot.output.width_bits > 16u)

    {

        *errorCode = "invalid_width";

        return false;

    }

    return true;

}



static bool GwMappingGetSlotQueryParam(const char *url, unsigned index, const char *key,

                                       char *out, size_t outLen)

{

    char prefixed[48]{0};

    snprintf(prefixed, sizeof(prefixed), "m%u_%s", index, key);

    return GetQueryParam(url, prefixed, out, outLen);

}



static bool GwMappingHasSlotQueryParam(const char *url, unsigned index, const char *key)

{

    char scratch[4]{0};

    return GwMappingGetSlotQueryParam(url, index, key, scratch, sizeof(scratch));

}



static bool GwMappingApplySaveFromUrl(const char *url, const char **errorCode, unsigned *savedCountOut)

{

    char countText[16]{0};

    char clearText[16]{0};

    LeapGatewayConfig next = g_gateway.config;

    *errorCode = nullptr;

    *savedCountOut = 0u;



    if (GetQueryParam(url, "clear", clearText, sizeof(clearText)) &&

        (strcmp(clearText, "1") == 0 || StrIStartsWith(clearText, "true")))

    {

        next.bridge.mapping_count = 0u;

        memset(next.bridge.mappings, 0, sizeof(next.bridge.mappings));

        if (leap_gateway_runtime_apply_config(&next) != 0)

        {

            *errorCode = "apply_failed";

            return false;

        }

        if (leap_gateway_storage_ready())

        {

            (void)leap_gateway_storage_save_config(&g_gateway.config);

        }

        leap_gateway_leap_session_request_auto_connect(&g_gateway);

        return true;

    }



    if (!GetQueryParam(url, "count", countText, sizeof(countText)))

    {

        *errorCode = "missing_count";

        return false;

    }



    const unsigned count = static_cast<unsigned>(strtoul(countText, nullptr, 10));

    if (count > GW_MAX_MAPPINGS)

    {

        *errorCode = "too_many_mappings";

        return false;

    }



    next.bridge.input_assembly_id = GW_INPUT_ASSEMBLY_ID;

    next.bridge.output_assembly_id = GW_OUTPUT_ASSEMBLY_ID;

    next.bridge.input_assembly_size = GW_ASSEMBLY_BYTES;

    next.bridge.output_assembly_size = GW_ASSEMBLY_BYTES;

    next.bridge.mapping_count = count;

    memset(next.bridge.mappings, 0, sizeof(next.bridge.mappings));



    for (unsigned i = 0; i < count; ++i)

    {

        LeapEipBridgeMapping &slot = next.bridge.mappings[i];

        GwMappingResetSlot(slot, i);



        char macText[32]{0};

        char profileText[32]{0};

        char inputText[16]{0};

        char outputText[16]{0};

        char statusText[16]{0};

        char widthText[16]{0};

        char enabledText[16]{0};



        if (!GwMappingGetSlotQueryParam(url, i, "mac", macText, sizeof(macText)) || !macText[0])

        {

            *errorCode = "missing_mac";

            return false;

        }

        if (!GwMappingParseMacText(macText, slot.leap_mac))

        {

            *errorCode = "invalid_mac";

            return false;

        }



        if (GwMappingGetSlotQueryParam(url, i, "profile", profileText, sizeof(profileText)) && profileText[0])

        {

            if (!GwMappingParseProfileText(profileText, slot.profile_id))

            {

                *errorCode = "invalid_profile";

                return false;

            }

        }



        if (GwMappingGetSlotQueryParam(url, i, "input_byte", inputText, sizeof(inputText)) && inputText[0])

        {

            const unsigned value = static_cast<unsigned>(strtoul(inputText, nullptr, 10));

            if (value >= GW_ASSEMBLY_BYTES)

            {

                *errorCode = "invalid_input_byte";

                return false;

            }

            slot.input.assembly_byte = static_cast<uint16_t>(value);

        }



        if (GwMappingGetSlotQueryParam(url, i, "output_byte", outputText, sizeof(outputText)) && outputText[0])

        {

            const unsigned value = static_cast<unsigned>(strtoul(outputText, nullptr, 10));

            if (value >= GW_ASSEMBLY_BYTES)

            {

                *errorCode = "invalid_output_byte";

                return false;

            }

            slot.output.assembly_byte = static_cast<uint16_t>(value);

        }



        if (GwMappingGetSlotQueryParam(url, i, "status_byte", statusText, sizeof(statusText)) && statusText[0])

        {

            const unsigned value = static_cast<unsigned>(strtoul(statusText, nullptr, 10));

            if (value >= GW_ASSEMBLY_BYTES)

            {

                *errorCode = "invalid_status_byte";

                return false;

            }

            slot.status_assembly_byte = static_cast<uint16_t>(value);

        }



        if (GwMappingGetSlotQueryParam(url, i, "width", widthText, sizeof(widthText)) && widthText[0])

        {

            const unsigned value = static_cast<unsigned>(strtoul(widthText, nullptr, 10));

            if (value < 1u || value > 16u)

            {

                *errorCode = "invalid_width";

                return false;

            }

            slot.input.width_bits = static_cast<uint8_t>(value);

            slot.output.width_bits = static_cast<uint8_t>(value);

        }



        if (GwMappingGetSlotQueryParam(url, i, "enabled", enabledText, sizeof(enabledText)))

        {

            slot.enabled =

                (strcmp(enabledText, "1") == 0 || StrIStartsWith(enabledText, "true")) ? 1 : 0;

        }

        else if (GwMappingHasSlotQueryParam(url, i, "mac"))

        {

            slot.enabled = 1;

        }



        const char *slotError = nullptr;

        if (!GwMappingValidateSlot(slot, &slotError))

        {

            *errorCode = slotError;

            return false;

        }

    }



    if (leap_gateway_runtime_apply_config(&next) != 0)

    {

        *errorCode = "apply_failed";

        return false;

    }



    if (leap_gateway_storage_ready())

    {

        (void)leap_gateway_storage_save_config(&g_gateway.config);

    }



    leap_gateway_leap_session_request_auto_connect(&g_gateway);

    *savedCountOut = count;

    return true;

}



static bool MatchMappingConfigApi(HTTP_Request &req)

{

    if (req.req != tGet || !req.pURL) return false;

    const char *url = req.pURL;

    while (*url == '/') ++url;

    const char *prefix = "api/mapping/config";

    const size_t n = strlen(prefix);

    if (strncmp(url, prefix, n) != 0) return false;

    const char tail = url[n];

    return (tail == '\0' || tail == '?' || tail == '#');

}



static bool MatchMappingConfigSaveApi(HTTP_Request &req)

{

    if (req.req != tGet || !req.pURL) return false;

    const char *url = req.pURL;

    while (*url == '/') ++url;

    const char *prefix = "api/mapping/config/save";

    const size_t n = strlen(prefix);

    if (strncmp(url, prefix, n) != 0) return false;

    const char tail = url[n];

    return (tail == '\0' || tail == '?' || tail == '#');

}



static int HandleMappingConfigApi(int sock, HTTP_Request &req)

{

    (void)req;



    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");

    fdprintf(sock,

             "{\"ok\":true,\"maxMappings\":%u,\"inputAssemblyId\":%u,\"outputAssemblyId\":%u,"

             "\"assemblyBytes\":%u,\"mappingCount\":%u,\"enabledCount\":%u,\"mappings\":[",

             GW_MAX_MAPPINGS,

             g_gateway.config.bridge.input_assembly_id,

             g_gateway.config.bridge.output_assembly_id,

             static_cast<unsigned>(g_gateway.config.bridge.input_assembly_size),

             g_gateway.config.bridge.mapping_count,

             GwMappingActiveCount());



    for (unsigned i = 0; i < g_gateway.config.bridge.mapping_count; ++i)

    {

        const LeapEipBridgeMapping &slot = g_gateway.config.bridge.mappings[i];

        if (i > 0u)

        {

            fdprintf(sock, ",");

        }

        fdprintf(sock, "{\"index\":%u,\"mac\":", i);

        GwMappingWriteMacJson(sock, slot.leap_mac);

        fdprintf(sock,

                 ",\"profile\":\"0x%08x\",\"inputByte\":%u,\"outputByte\":%u,\"statusByte\":%u,"

                 "\"width\":%u,\"enabled\":%s}",

                 slot.profile_id,

                 slot.input.assembly_byte,

                 slot.output.assembly_byte,

                 slot.status_assembly_byte,

                 slot.input.width_bits,

                 slot.enabled ? "true" : "false");

    }



    fdprintf(sock, "]}");

    return 1;

}



static int HandleMappingConfigSaveApi(int sock, HTTP_Request &req)

{

    const char *errorCode = nullptr;

    unsigned savedCount = 0u;



    if (!GwMappingApplySaveFromUrl(req.pURL, &errorCode, &savedCount))

    {

        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");

        fdprintf(sock, "{\"ok\":false,\"error\":\"%s\"}", errorCode ? errorCode : "invalid_request");

        return 1;

    }



    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");

    fdprintf(sock,

             "{\"ok\":true,\"saved\":true,\"mappingCount\":%u,\"enabledCount\":%u,"

             "\"message\":\"Mappings saved and LEAP connect requested.\"}",

             g_gateway.config.bridge.mapping_count,

             GwMappingActiveCount());

    return 1;

}



#endif // LEAPGATEWAY_MAIN_TU

