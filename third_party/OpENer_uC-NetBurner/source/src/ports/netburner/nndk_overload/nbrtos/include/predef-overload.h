/*******************************************************************************
 * OpENer_uC-NetBurner — NetBurner NNDK predef overrides (Extended tier LLDP/ACD)
 *
 * NBEclipse overload mirror path: overload/nbrtos/include/predef-overload.h
 * (same content as ../predef-overload.h at nndk_overload root).
 *
 * ALLOW_CUSTOM_NET_DO_RX — LLDP RX via SetCustomNetDoRX (see nbrtos/source/netrx.cpp)
 * ENABLE_SNMP — required by NNDK LLDPEntity TX path
 ******************************************************************************/

#ifndef ENABLE_SNMP
#define ENABLE_SNMP (1)
#endif

#ifndef ALLOW_CUSTOM_NET_DO_RX
#define ALLOW_CUSTOM_NET_DO_RX (1)
#endif
