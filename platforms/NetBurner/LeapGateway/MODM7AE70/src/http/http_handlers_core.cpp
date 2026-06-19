/*
 * LEAP Gateway (NetBurner MOD5441X)
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef LEAPGATEWAY_MAIN_TU

static bool ParseIpv4Text(const char *text, IPADDR4 &ipOut);
static bool StrIStartsWith(const char *text, const char *prefix);

static const char *NormalizeIpv4Mode(const char *mode)
{
    if (!mode || !mode[0]) return nullptr;
    if (strcmp(mode, "DHCP") == 0) return "DHCP";
    if (strcmp(mode, "Static") == 0) return "Static";
    if (strcmp(mode, "Disabled") == 0) return "Disabled";
    if (strcmp(mode, "DHCP w Fallback") == 0 || strcmp(mode, "DHCP w/ Fallback") == 0) return "DHCP w Fallback";
    return nullptr;
}

static bool MatchNetworkConfigApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/network/config";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool MatchNetworkConfigSaveApi(HTTP_Request &req)
{
    if (req.req != tGet || !req.pURL) return false;
    const char *url = req.pURL;
    while (*url == '/') ++url;
    const char *prefix = "api/network/config/save";
    const size_t n = strlen(prefix);
    if (strncmp(url, prefix, n) != 0) return false;
    const char tail = url[n];
    return (tail == '\0' || tail == '?' || tail == '#');
}

static bool GetNetworkQueryParam(const char *url, int ifNumber, bool allowUnprefixedFallback, const char *key,
                                 char *out, size_t outLen)
{
    char prefixed[48]{0};
    snprintf(prefixed, sizeof(prefixed), "if%d_%s", ifNumber, key);
    if (GetQueryParam(url, prefixed, out, outLen))
    {
        return true;
    }
    if (allowUnprefixedFallback && GetQueryParam(url, key, out, outLen))
    {
        return true;
    }
    return false;
}

static bool HasNetworkQueryParam(const char *url, int ifNumber, bool allowUnprefixedFallback, const char *key)
{
    char scratch[4]{0};
    return GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, key, scratch, sizeof(scratch));
}

static void WriteInterfaceJson(int sock, int ifNumber, bool prependComma)
{
    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock)
    {
        return;
    }

    if (prependComma)
    {
        fdprintf(sock, ",");
    }

    fdprintf(sock,
             "{\"interface\":%d,\"port\":%d,\"role\":\"%s\",\"portLabel\":\"%s\",\"name\":\"%s\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"%hI\",\"activeMask\":\"%hI\",\"activeGate\":\"%hI\","
             "\"activeDNS1\":\"%hI\",\"activeDNS2\":\"%hI\",\"autoIPAddr\":\"%hI\"}",
             ifNumber,
             ifNumber,
             GetInterfaceRole(ifNumber),
             GetInterfacePortLabel(ifNumber),
             ifBlock->GetInterfaceName(),
             static_cast<NBString>(ifBlock->ip4.mode).c_str(),
             static_cast<IPADDR4>(ifBlock->ip4.addr),
             static_cast<IPADDR4>(ifBlock->ip4.mask),
             static_cast<IPADDR4>(ifBlock->ip4.gate),
             static_cast<IPADDR4>(ifBlock->ip4.dns1),
             static_cast<IPADDR4>(ifBlock->ip4.dns2),
             InterfaceIP(ifNumber),
             InterfaceMASK(ifNumber),
             InterfaceGate(ifNumber),
             InterfaceDNS(ifNumber),
             InterfaceDNS2(ifNumber),
             InterfaceAutoIP(ifNumber));
}

static bool ApplyInterfaceNetworkSave(const char *url, int ifNumber, bool allowUnprefixedFallback,
                                      InterfaceBlock *ifBlock, const char **errorCode)
{
    char modeText[32]{0};
    char ipText[32]{0};
    char maskText[32]{0};
    char gateText[32]{0};
    char dns1Text[32]{0};
    char dns2Text[32]{0};

    *errorCode = nullptr;

    if (!HasNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mode"))
    {
        return false;
    }

    if (!GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mode", modeText, sizeof(modeText)))
    {
        *errorCode = "missing_mode";
        return false;
    }

    const char *mode = NormalizeIpv4Mode(modeText);
    if (!mode)
    {
        *errorCode = "invalid_mode";
        return false;
    }

    IPADDR4 staticIp{};
    IPADDR4 staticMask{};
    IPADDR4 staticGate{};
    IPADDR4 staticDns1{};
    IPADDR4 staticDns2{};

    const bool needsStatic = (strcmp(mode, "Static") == 0 || strcmp(mode, "DHCP w Fallback") == 0);
    if (needsStatic)
    {
        if (GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "ip", ipText, sizeof(ipText)) && ipText[0])
        {
            if (!ParseIpv4Text(ipText, staticIp) || staticIp.IsNull())
            {
                *errorCode = "invalid_static_ip";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_ip";
            return false;
        }

        if (GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "mask", maskText, sizeof(maskText)) && maskText[0])
        {
            if (!ParseIpv4Text(maskText, staticMask) || staticMask.IsNull())
            {
                *errorCode = "invalid_static_mask";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_mask";
            return false;
        }
    }

    const bool haveGate = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "gateway", gateText, sizeof(gateText));
    const bool haveDns1 = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "dns1", dns1Text, sizeof(dns1Text));
    const bool haveDns2 = needsStatic &&
                          GetNetworkQueryParam(url, ifNumber, allowUnprefixedFallback, "dns2", dns2Text, sizeof(dns2Text));

    if (haveGate && gateText[0] && strcmp(gateText, "0.0.0.0") != 0 && !ParseIpv4Text(gateText, staticGate))
    {
        *errorCode = "invalid_static_gateway";
        return false;
    }
    if (haveDns1 && dns1Text[0] && strcmp(dns1Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns1Text, staticDns1))
    {
        *errorCode = "invalid_static_dns1";
        return false;
    }
    if (haveDns2 && dns2Text[0] && strcmp(dns2Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns2Text, staticDns2))
    {
        *errorCode = "invalid_static_dns2";
        return false;
    }

    ifBlock->ip4.mode = mode;
    if (needsStatic)
    {
        ifBlock->ip4.addr = staticIp;
        ifBlock->ip4.mask = staticMask;
        if (haveGate) ifBlock->ip4.gate = staticGate;
        if (haveDns1) ifBlock->ip4.dns1 = staticDns1;
        if (haveDns2) ifBlock->ip4.dns2 = staticDns2;
    }
    return true;
}

static bool ApplyEnet1NetworkSave(const char *url, const char **errorCode)
{
    if (!IsDualEthernetModule())
    {
        return false;
    }

    char modeText[32]{0};
    char ipText[32]{0};
    char maskText[32]{0};
    char gateText[32]{0};
    char dns1Text[32]{0};
    char dns2Text[32]{0};

    *errorCode = nullptr;

    if (!HasNetworkQueryParam(url, 2, false, "mode"))
    {
        return false;
    }

    if (!GetNetworkQueryParam(url, 2, false, "mode", modeText, sizeof(modeText)))
    {
        *errorCode = "missing_mode";
        return false;
    }

    const char *mode = NormalizeIpv4Mode(modeText);
    if (!mode)
    {
        *errorCode = "invalid_mode";
        return false;
    }

    IPADDR4 staticIp{};
    IPADDR4 staticMask{};
    IPADDR4 staticGate{};
    IPADDR4 staticDns1{};
    IPADDR4 staticDns2{};

    const bool needsStatic = (strcmp(mode, "Static") == 0 || strcmp(mode, "DHCP w Fallback") == 0);
    if (needsStatic)
    {
        if (GetNetworkQueryParam(url, 2, false, "ip", ipText, sizeof(ipText)) && ipText[0])
        {
            if (!ParseIpv4Text(ipText, staticIp) || staticIp.IsNull())
            {
                *errorCode = "invalid_static_ip";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_ip";
            return false;
        }

        if (GetNetworkQueryParam(url, 2, false, "mask", maskText, sizeof(maskText)) && maskText[0])
        {
            if (!ParseIpv4Text(maskText, staticMask) || staticMask.IsNull())
            {
                *errorCode = "invalid_static_mask";
                return false;
            }
        }
        else
        {
            *errorCode = "missing_static_mask";
            return false;
        }
    }

    const bool haveGate = needsStatic && GetNetworkQueryParam(url, 2, false, "gateway", gateText, sizeof(gateText));
    const bool haveDns1 = needsStatic && GetNetworkQueryParam(url, 2, false, "dns1", dns1Text, sizeof(dns1Text));
    const bool haveDns2 = needsStatic && GetNetworkQueryParam(url, 2, false, "dns2", dns2Text, sizeof(dns2Text));

    if (haveGate && gateText[0] && strcmp(gateText, "0.0.0.0") != 0 && !ParseIpv4Text(gateText, staticGate))
    {
        *errorCode = "invalid_static_gateway";
        return false;
    }
    if (haveDns1 && dns1Text[0] && strcmp(dns1Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns1Text, staticDns1))
    {
        *errorCode = "invalid_static_dns1";
        return false;
    }
    if (haveDns2 && dns2Text[0] && strcmp(dns2Text, "0.0.0.0") != 0 && !ParseIpv4Text(dns2Text, staticDns2))
    {
        *errorCode = "invalid_static_dns2";
        return false;
    }

    InterfaceBlock *ifBlock = GetInterfaceBlock(2);
    if (!ifBlock)
    {
        *errorCode = "interface_not_available";
        return false;
    }

    ifBlock->ip4.mode = mode;
    if (needsStatic)
    {
        ifBlock->ip4.addr = staticIp;
        ifBlock->ip4.mask = staticMask;
        if (haveGate) ifBlock->ip4.gate = staticGate;
        if (haveDns1) ifBlock->ip4.dns1 = staticDns1;
        if (haveDns2) ifBlock->ip4.dns2 = staticDns2;
    }
    return true;
}

static void WriteEnet1PreviewJson(int sock, bool prependComma)
{
    if (prependComma)
    {
        fdprintf(sock, ",");
    }

    const char *mode = "DHCP";
    NBString modeStorage;
    IPADDR4 staticAddr{};
    IPADDR4 staticMask{};
    IPADDR4 staticGate{};
    IPADDR4 staticDns1{};
    IPADDR4 staticDns2{};
    InterfaceBlock *ifBlock = GetInterfaceBlock(2);
    if (ifBlock)
    {
        modeStorage = static_cast<NBString>(ifBlock->ip4.mode);
        mode = modeStorage.c_str();
        staticAddr = static_cast<IPADDR4>(ifBlock->ip4.addr);
        staticMask = static_cast<IPADDR4>(ifBlock->ip4.mask);
        staticGate = static_cast<IPADDR4>(ifBlock->ip4.gate);
        staticDns1 = static_cast<IPADDR4>(ifBlock->ip4.dns1);
        staticDns2 = static_cast<IPADDR4>(ifBlock->ip4.dns2);
    }

    fdprintf(sock,
             "{\"interface\":2,\"port\":2,\"role\":\"leap\",\"portLabel\":\"Port 2 - LEAP Network\","
             "\"name\":\"Ethernet1\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"0.0.0.0\",\"activeMask\":\"0.0.0.0\",\"activeGate\":\"0.0.0.0\","
             "\"activeDNS1\":\"0.0.0.0\",\"activeDNS2\":\"0.0.0.0\",\"autoIPAddr\":\"0.0.0.0\"}",
             mode,
             staticAddr,
             staticMask,
             staticGate,
             staticDns1,
             staticDns2);
}

static int HandleNetworkConfigApi(int sock, HTTP_Request &req)
{
    (void)req;
    const int firstIfNumber = GetFirstInterface();
    InterfaceBlock *firstIfBlock = firstIfNumber ? GetInterfaceBlock(firstIfNumber) : nullptr;
    if (!firstIfBlock)
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"no_interface\"}");
        return 1;
    }

    const bool dualEthernetSupported = IsDualEthernetModule();

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"dualEthernetSupported\":%s,\"topology\":\"%s\","
             "\"plantPort\":1,\"leapPort\":2,\"interfaces\":[",
             dualEthernetSupported ? "true" : "false",
             dualEthernetSupported ? "dual_independent" : "single");

    bool wroteAny = false;
    int interfaceCount = 0;
    for (int ifNumber = firstIfNumber; ifNumber; ifNumber = GetNextInterface(ifNumber))
    {
        WriteInterfaceJson(sock, ifNumber, wroteAny);
        wroteAny = true;
        ++interfaceCount;
    }

    if (dualEthernetSupported && interfaceCount < 2)
    {
        WriteEnet1PreviewJson(sock, wroteAny);
    }

    fdprintf(sock,
             "],\"interface\":%d,\"name\":\"%s\",\"mode\":\"%s\","
             "\"staticAddr\":\"%hI\",\"staticMask\":\"%hI\",\"staticGate\":\"%hI\","
             "\"staticDNS1\":\"%hI\",\"staticDNS2\":\"%hI\","
             "\"activeAddr\":\"%hI\",\"activeMask\":\"%hI\",\"activeGate\":\"%hI\","
             "\"activeDNS1\":\"%hI\",\"activeDNS2\":\"%hI\",\"autoIPAddr\":\"%hI\"}",
             firstIfNumber,
             firstIfBlock->GetInterfaceName(),
             static_cast<NBString>(firstIfBlock->ip4.mode).c_str(),
             static_cast<IPADDR4>(firstIfBlock->ip4.addr),
             static_cast<IPADDR4>(firstIfBlock->ip4.mask),
             static_cast<IPADDR4>(firstIfBlock->ip4.gate),
             static_cast<IPADDR4>(firstIfBlock->ip4.dns1),
             static_cast<IPADDR4>(firstIfBlock->ip4.dns2),
             InterfaceIP(firstIfNumber),
             InterfaceMASK(firstIfNumber),
             InterfaceGate(firstIfNumber),
             InterfaceDNS(firstIfNumber),
             InterfaceDNS2(firstIfNumber),
             InterfaceAutoIP(firstIfNumber));
    return 1;
}

static int HandleNetworkConfigSaveApi(int sock, HTTP_Request &req)
{
    char rebootText[16]{0};
    char etherSwitchText[16]{0};

    const int firstIfNumber = GetFirstInterface();
    if (!firstIfNumber)
    {
        fdprintf(sock, "HTTP/1.0 503 Service Unavailable\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"no_interface\"}");
        return 1;
    }

    if (GetQueryParam(req.pURL, "etherSwitch", etherSwitchText, sizeof(etherSwitchText)))
    {
        const bool enableSwitch = (strcmp(etherSwitchText, "1") == 0 || StrIStartsWith(etherSwitchText, "true"));
        if (enableSwitch)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"bridge_mode_not_supported\"}");
            return 1;
        }
    }

    bool topologyChanged = false;
    ForceIndependentDualEthernet(&topologyChanged);

    bool anyInterfaceUpdated = false;
    bool secondInterfaceRegistered = false;
    for (int ifNumber = firstIfNumber; ifNumber; ifNumber = GetNextInterface(ifNumber))
    {
        InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
        if (!ifBlock)
        {
            continue;
        }

        if (ifNumber != firstIfNumber)
        {
            secondInterfaceRegistered = true;
        }

        const bool allowUnprefixedFallback = (ifNumber == firstIfNumber);
        const char *ifError = nullptr;
        if (ApplyInterfaceNetworkSave(req.pURL, ifNumber, allowUnprefixedFallback, ifBlock, &ifError))
        {
            anyInterfaceUpdated = true;
        }
        else if (ifError)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"%s\",\"interface\":%d}", ifError, ifNumber);
            return 1;
        }
    }

    if (!secondInterfaceRegistered)
    {
        const char *enet1Error = nullptr;
        if (ApplyEnet1NetworkSave(req.pURL, &enet1Error))
        {
            anyInterfaceUpdated = true;
        }
        else if (enet1Error)
        {
            fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
            fdprintf(sock, "{\"ok\":false,\"error\":\"%s\",\"interface\":2}", enet1Error);
            return 1;
        }
    }

    if (!anyInterfaceUpdated && !topologyChanged)
    {
        fdprintf(sock, "HTTP/1.0 400 Bad Request\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
        fdprintf(sock, "{\"ok\":false,\"error\":\"missing_mode\"}");
        return 1;
    }

    if (anyInterfaceUpdated || topologyChanged)
    {
        SaveConfigToStorage();
    }

    const bool reboot = GetQueryParam(req.pURL, "reboot", rebootText, sizeof(rebootText)) &&
                        (strcmp(rebootText, "1") == 0 || StrIStartsWith(rebootText, "true"));

    fdprintf(sock, "HTTP/1.0 200 OK\r\nPragma: no-cache\r\nContent-Type: application/json\r\n\r\n");
    fdprintf(sock,
             "{\"ok\":true,\"saved\":true,\"rebooting\":%s,"
             "\"message\":\"Saved to flash. Reboot required for IPv4 changes to take effect.\"}",
             reboot ? "true" : "false");
    if (reboot)
    {
        OSTimeDly(TICKS_PER_SECOND / 2);
        ForceReboot();
    }
    return 1;
}

#endif // LEAPGATEWAY_MAIN_TU
