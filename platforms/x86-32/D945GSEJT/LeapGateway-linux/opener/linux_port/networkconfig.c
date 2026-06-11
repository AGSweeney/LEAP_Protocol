/*******************************************************************************
 * Linux network configuration for LeapOS-Gateway OpENer.
 *
 * Minimal appliance profile: MAC + IPv4/mask via ioctl only (no resolv.conf or
 * /proc/net/route — those may be absent on the Alpine gateway image).
 ******************************************************************************/

#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "trace.h"
#include "opener_api.h"

EipStatus IfaceGetMacAddress(const char *iface,
                             uint8_t *const physical_address) {
  struct ifreq ifr;
  size_t if_name_len = strlen(iface);
  EipStatus status = kEipStatusError;
  int fd;

  if (if_name_len >= sizeof(ifr.ifr_name)) {
    errno = ENAMETOOLONG;
    return kEipStatusError;
  }

  memcpy(ifr.ifr_name, iface, if_name_len);
  ifr.ifr_name[if_name_len] = '\0';

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (fd < 0) {
    return kEipStatusError;
  }

  if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
    memcpy(physical_address, ifr.ifr_hwaddr.sa_data, 6);
    status = kEipStatusOk;
  }

  close(fd);
  return status;
}

static EipStatus GetIpAndNetmaskFromInterface(const char *iface,
                                              CipTcpIpInterfaceConfiguration *iface_cfg) {
  struct ifreq ifr;
  size_t if_name_len = strlen(iface);
  int fd;
  EipStatus status = kEipStatusError;

  if (if_name_len >= sizeof(ifr.ifr_name)) {
    return kEipStatusError;
  }

  memcpy(ifr.ifr_name, iface, if_name_len);
  ifr.ifr_name[if_name_len] = '\0';

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (fd < 0) {
    return kEipStatusError;
  }

  if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
    iface_cfg->ip_address =
      ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    status = kEipStatusOk;
  }

  if (status == kEipStatusOk &&
      ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
    iface_cfg->network_mask =
      ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr;
  } else if (status == kEipStatusOk) {
    status = kEipStatusError;
  }

  close(fd);
  return status;
}

EipStatus IfaceGetConfiguration(const char *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  EipStatus status;

  memset(&local_cfg, 0, sizeof(local_cfg));
  status = GetIpAndNetmaskFromInterface(iface, &local_cfg);
  if (status == kEipStatusOk) {
    ClearCipString(&iface_cfg->domain_name);
    *iface_cfg = local_cfg;
  }
  return status;
}

EipStatus IfaceWaitForIp(const char *const iface,
                         int timeout,
                         volatile int *const p_abort_wait) {
  (void)iface;
  (void)timeout;
  (void)p_abort_wait;
  return kEipStatusOk;
}

void GetHostName(CipString *hostname) {
  SetCipStringByCstr(hostname, "LeapOS-Gateway");
}
