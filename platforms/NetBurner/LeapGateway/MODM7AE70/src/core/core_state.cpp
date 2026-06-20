/*
 * LEAP Gateway (NetBurner MOD5441X)
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

#ifdef LEAPGATEWAY_MAIN_TU
const char *AppName = "LEAP Gateway";
static bool g_gateway_forced_independent_topology = false;

static uint32_t GatewaySwapIpv4Bytes(uint32_t value)
{
    return ((value & 0x000000FFUL) << 24) |
           ((value & 0x0000FF00UL) << 8) |
           ((value & 0x00FF0000UL) >> 8) |
           ((value & 0xFF000000UL) >> 24);
}

static bool IsDualEthernetModule()
{
    int count = 0;
    for (int ifNumber = GetFirstInterface(); ifNumber; ifNumber = GetNextInterface(ifNumber))
    {
        ++count;
        if (count >= 2)
        {
            return true;
        }
    }
    return false;
}

static void EnsureGatewayPortTopology()
{
    if (!IsDualEthernetModule())
    {
        return;
    }

    if (g_gateway_forced_independent_topology)
    {
        SaveConfigToStorage();
        g_gateway_forced_independent_topology = false;
        iprintf("Ethernet bridge mode disabled before network init.\r\n");
    }

}

static void ForceIndependentDualEthernet(bool *topologyChanged)
{
    if (!IsDualEthernetModule())
    {
        return;
    }

    (void)topologyChanged;
}

static const char *GetInterfaceRole(int ifNumber)
{
    if (ifNumber == 1)
    {
        return "plant";
    }
    if (ifNumber == 2)
    {
        return "leap";
    }
    return "unknown";
}

static const char *GetInterfacePortLabel(int ifNumber)
{
    if (!IsDualEthernetModule())
    {
        return "Primary Network Interface";
    }
    if (ifNumber == 1)
    {
        return "Port 1 - Plant Network";
    }
    if (ifNumber == 2)
    {
        return "Port 2 - LEAP Network";
    }
    return "Network";
}

static IPADDR4 GetInterfaceIpv4Address(int ifNumber)
{
    if (!ifNumber)
    {
        return IPADDR4{};
    }
    IPADDR4 ip = InterfaceIP(ifNumber);
    if (!ip.IsNull())
    {
        return ip;
    }
#ifdef AUTOIP
    ip = InterfaceAutoIP(ifNumber);
    if (!ip.IsNull())
    {
        return ip;
    }
#endif
    return IPADDR4{};
}

static void EnsureAutoIpEnabled()
{
    const int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        return;
    }
    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock)
    {
        return;
    }
    if (!static_cast<bool>(ifBlock->ip4.autoip))
    {
        ifBlock->ip4.autoip = true;
        iprintf("Enabled AutoIP fallback for %s\r\n", ifBlock->GetInterfaceName());
    }
}

#ifdef AUTOIP
static void KickAutoIpIfNeeded(int ifNumber)
{
    if (!ifNumber || !InterfaceIP(ifNumber).IsNull())
    {
        return;
    }
    if (!InterfaceAutoIP(ifNumber).IsNull())
    {
        return;
    }

    InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);
    if (!ifBlock || !static_cast<bool>(ifBlock->ip4.autoip))
    {
        return;
    }

    iprintf("No DHCP address; starting AutoIP negotiation (169.254.x.x)...\r\n");
    ifBlock->AutoClient.restart();
}
#endif

static bool WaitForNetworkWithAutoIpFallback()
{
    const int ifNumber = GetFirstInterface();
    if (!ifNumber)
    {
        return false;
    }

    EnsureAutoIpEnabled();

    if (WaitForActiveNetwork(TICKS_PER_SECOND * 30))
    {
        return true;
    }

#ifdef AUTOIP
    KickAutoIpIfNeeded(ifNumber);

    iprintf("Waiting up to 30s for AutoIP link-local address...\r\n");
    const uint32_t deadline = TimeTick + (TICKS_PER_SECOND * 30);
    while (TimeTick < deadline)
    {
        if (HaveActiveNetwork(ifNumber))
        {
            return true;
        }

        const IPADDR4 ip = GetInterfaceIpv4Address(ifNumber);
        if (!ip.IsNull())
        {
            iprintf("AutoIP ready: %hI\r\n", ip);
            return true;
        }
        OSTimeDly(TICKS_PER_SECOND / 4);
    }
#endif

    return false;
}

static bool UrlDecode(const char *in, char *out, size_t outSize)
{
    if (!in || !out || outSize < 2)
    {
        return false;
    }

    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < outSize; ++i)
    {
        if (in[i] == '+')
        {
            out[o++] = ' ';
        }
        else if (in[i] == '%' && isxdigit(static_cast<unsigned char>(in[i + 1])) && isxdigit(static_cast<unsigned char>(in[i + 2])))
        {
            char hex[3] = {in[i + 1], in[i + 2], '\0'};
            out[o++] = static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        }
        else
        {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
    return true;
}

static bool GetQueryParam(const char *url, const char *key, char *out, size_t outSize)
{
    if (!url || !key || !out || outSize < 2)
    {
        return false;
    }

    const char *q = strchr(url, '?');
    if (!q)
    {
        return false;
    }
    ++q;
    const size_t keyLen = strlen(key);

    while (*q)
    {
        const char *amp = strchr(q, '&');
        const char *end = amp ? amp : (q + strlen(q));
        const char *eq = static_cast<const char *>(memchr(q, '=', static_cast<size_t>(end - q)));
        if (eq)
        {
            const size_t klen = static_cast<size_t>(eq - q);
            if (klen == keyLen && strncmp(q, key, keyLen) == 0)
            {
                char encoded[1024]{0};
                size_t vlen = static_cast<size_t>(end - (eq + 1));
                if (vlen >= sizeof(encoded))
                {
                    vlen = sizeof(encoded) - 1;
                }
                memcpy(encoded, eq + 1, vlen);
                encoded[vlen] = '\0';
                return UrlDecode(encoded, out, outSize);
            }
        }
        if (!amp)
        {
            break;
        }
        q = amp + 1;
    }
    return false;
}

static bool ParseIpv4Text(const char *text, IPADDR4 &ipOut)
{
    if (!text || !text[0])
    {
        return false;
    }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
    {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255)
    {
        return false;
    }
    const uint32_t raw = (static_cast<uint32_t>(a) << 24) |
                         (static_cast<uint32_t>(b) << 16) |
                         (static_cast<uint32_t>(c) << 8) |
                         static_cast<uint32_t>(d);
    ipOut = IPADDR4(GatewaySwapIpv4Bytes(raw));
    return !ipOut.IsNull();
}

static bool StrIStartsWith(const char *text, const char *prefix)
{
    if (!text || !prefix)
    {
        return false;
    }
    while (*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z')
        {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

#endif // LEAPGATEWAY_MAIN_TU
