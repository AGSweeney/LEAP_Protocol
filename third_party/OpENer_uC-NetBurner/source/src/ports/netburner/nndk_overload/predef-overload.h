/*******************************************************************************
 * OpENer_uC-NetBurner — NetBurner NNDK predef overrides (Extended tier LLDP/ACD)
 *
 * Include via NBEclipse USER_PREDEF or overload/nbrtos/include/predef-overload.h.
 * Mirror copy: nbrtos/include/predef-overload.h (keep in sync).
 *
 * ALLOW_CUSTOM_NET_DO_RX — LLDP RX via SetCustomNetDoRX + netrx.cpp overload
 * ENABLE_SNMP — required by NNDK LLDPEntity path used for TX
 ******************************************************************************/

#ifndef ENABLE_SNMP
#define ENABLE_SNMP (1)
#endif

#ifndef ALLOW_CUSTOM_NET_DO_RX
#define ALLOW_CUSTOM_NET_DO_RX (1)
#endif
