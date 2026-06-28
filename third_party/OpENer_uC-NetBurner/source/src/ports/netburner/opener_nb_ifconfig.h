/*******************************************************************************
 * OpENer_uC-NetBurner — NetBurner interface number and NNDK ifconfig helpers
 *
 * NNDK uses 1-based interface numbers. Pass (OpenerNetIfHandle)(intptr_t)ifnum to
 * opener_init() and OpenerHal_NetInit(). Use OpenerNbIface* to read PHY and IP mode
 * when extending networkconfig.c or custom SetAttribute handlers.
 ******************************************************************************/
#ifndef OPENER_NB_IFCONFIG_H_
#define OPENER_NB_IFCONFIG_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IPv4 addresses in CIP byte order (CipUdint layout). */
typedef struct OpenerNbIpv4Config {
  uint32_t ip;
  uint32_t mask;
  uint32_t gateway;
  uint32_t dns1;
  uint32_t dns2;
} OpenerNbIpv4Config;

typedef enum OpenerNbIpConfigMethod {
  kOpenerNbIpConfigStatic = 0,
  kOpenerNbIpConfigDhcp = 1,
  kOpenerNbIpConfigUnknown = 2
} OpenerNbIpConfigMethod;

/** @brief Parse decimal interface name ("1", "2") to ifnum; 0 on failure. */
int OpenerNbIfaceNumberFromName(const char *iface_name);

/** @brief Read MAC from InterfaceMAC(); returns 0 on success. */
int OpenerNbIfaceGetMac(int ifnum, uint8_t *mac_out);

/** @brief Read IPv4, mask, gateway, DNS; returns 0 when IP is assigned. */
int OpenerNbIfaceGetIpv4Config(int ifnum, OpenerNbIpv4Config *cfg_out);

/** @brief Map NNDK InterfaceBlock ip4.mode to static/DHCP for g_tcpip.config_control. */
OpenerNbIpConfigMethod OpenerNbIfaceGetIpConfigMethod(int ifnum);

/** @brief Copy NNDK interface name into caller buffer (NUL-terminated). Returns 0 on success. */
int OpenerNbIfaceGetHostName(int ifnum, char *buf, size_t buf_len);

/** @brief Update persisted static IPv4 settings from CIP values; returns 0 on success. */
int OpenerNbIfaceSetIpv4Config(int ifnum, const OpenerNbIpv4Config *cfg_in);

/** @brief Set persisted IP mode (Static/DHCP) in NNDK config; returns 0 on success. */
int OpenerNbIfaceSetIpConfigMethod(int ifnum, OpenerNbIpConfigMethod method);

/** @brief Set persisted device host name in NNDK config; returns 0 on success. */
int OpenerNbIfaceSetHostName(int ifnum, const char *host_name);

/** @brief Save config and apply mode-specific runtime values; returns 0 on success. */
int OpenerNbIfaceCommitConfig(int ifnum, OpenerNbIpConfigMethod method);

/** @brief Push link speed, duplex, and active flag into CIP Ethernet Link instance 1. */
void OpenerNbSyncEthernetLinkFromNndk(int ifnum);

/** @brief Convert NNDK IPADDR4 native layout to OpENer CIP IPv4 (CipUdint). */
uint32_t OpenerNbIpv4ToCip(uint32_t nb_ipv4);

/** @brief Convert OpENer CIP IPv4 (CipUdint) to NNDK IPADDR4 native layout. */
uint32_t OpenerNbCipToIpv4(uint32_t cip_ipv4);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_IFCONFIG_H_ */
