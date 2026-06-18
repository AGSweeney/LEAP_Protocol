/*******************************************************************************
 * BSD-to-NetBurner socket shim for OpENer.
 ******************************************************************************/

#include <predef.h>
#include <tcp.h>
#include <udp.h>
#include <netinterface.h>
#include <constants.h>

#define NB_PEER_TABLE_SIZE 32

extern "C" {
#include <errno.h>
#include <string.h>
#include "opener_bsd_compat.h"
#include "opener_nb_socket.h"
}

extern int g_opener_plant_ifnum;

extern "C" int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                       unsigned long timeout);

extern "C" int nb_sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                             unsigned long timeout) {
  return select(nfds, readfds, writefds, errorfds, timeout);
}

struct NbPeerInfo {
  int in_use;
  uint32_t addr_be;
  uint16_t port;
};

static NbPeerInfo g_peer_table[NB_PEER_TABLE_SIZE];

static NbPeerInfo *peer_slot_for_fd(int fd) {
  if ((fd < 0) || (fd >= NB_PEER_TABLE_SIZE)) {
    return NULL;
  }
  return &g_peer_table[fd];
}

extern "C" void nb_socket_store_peer(int sockfd, uint32_t peer_addr_be, uint16_t peer_port) {
  NbPeerInfo *slot = peer_slot_for_fd(sockfd);
  if (slot != NULL) {
    slot->in_use = 1;
    slot->addr_be = peer_addr_be;
    slot->port = peer_port;
  }
}

static IPADDR4 ipaddr4_from_opener(uint32_t addr) {
  if (addr == 0U) {
    return IPADDR4::NullIP();
  }
  return IPADDR4(addr);
}

extern "C" int nb_tcp_listen(uint16_t port, int backlog) {
  const uint8_t max_pending = (backlog > 0) ? (uint8_t)backlog : (uint8_t)5;
  const int ifnum = (g_opener_plant_ifnum > 0) ? g_opener_plant_ifnum : 1;
  const IPADDR4 iface_ip = InterfaceIP(ifnum);

  if (iface_ip.IsNull()) {
    errno = EIO;
    return -1;
  }

  /* INADDR_ANY + interface IP: NetBurner rejects specific his_ip binds for SYN match. */
  const int fd = listenvia4(IPADDR4::NullIP(), port, iface_ip, max_pending);
  if (fd < 0) {
    errno = EIO;
  }
  return fd;
}

extern "C" int nb_udp_listen(uint16_t port) {
  const int ifnum = (g_opener_plant_ifnum > 0) ? g_opener_plant_ifnum : 1;
  /* Rx+Tx: OpENer replies to List Identity via sendto on the same UDP fd. */
  const int fd = CreateRxTxUdpSocketVia4(IPADDR4::NullIP(), 0, port, ifnum);
  if (fd < 0) {
    errno = EIO;
  }
  return fd;
}

extern "C" int nb_udp_io_socket(uint16_t port) {
  const int ifnum = (g_opener_plant_ifnum > 0) ? g_opener_plant_ifnum : 1;
  const int fd = CreateRxTxUdpSocketVia4(IPADDR4::NullIP(), 0, port, ifnum);
  if (fd < 0) {
    errno = EIO;
  }
  return fd;
}

extern "C" int opener_nb_socket(int domain, int type, int protocol) {
  (void)protocol;

  if (domain != AF_INET) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  if (type == SOCK_DGRAM) {
    return nb_udp_listen(0);
  }

  if (type == SOCK_STREAM) {
    errno = EOPNOTSUPP;
    return -1;
  }

  errno = EPROTOTYPE;
  return -1;
}

extern "C" int opener_nb_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  return 0;
}

extern "C" int opener_nb_listen(int sockfd, int backlog) {
  (void)sockfd;
  (void)backlog;
  return 0;
}

extern "C" int opener_nb_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  IPADDR4 peer_ip;
  uint16_t peer_port = 0U;
  const int client_fd = accept4(sockfd, &peer_ip, &peer_port, (uint16_t)0);

  if (client_fd < 0) {
    if (client_fd == TCP_ERR_TIMEOUT) {
      errno = EWOULDBLOCK;
    } else {
      errno = EIO;
    }
    return -1;
  }

  if ((addr != NULL) && (addrlen != NULL) && (*addrlen >= (socklen_t)sizeof(struct sockaddr_in))) {
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(peer_port);
    sin->sin_addr.s_addr = (uint32_t)peer_ip;
    *addrlen = (socklen_t)sizeof(struct sockaddr_in);
  }

  nb_socket_store_peer(client_fd, (uint32_t)peer_ip, peer_port);
  return client_fd;
}

extern "C" int opener_nb_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  errno = EOPNOTSUPP;
  return -1;
}

extern "C" ssize_t opener_nb_send(int sockfd, const void *buf, size_t len, int flags) {
  (void)flags;
  return (ssize_t)write(sockfd, (const char *)buf, (int)len);
}

extern "C" ssize_t opener_nb_recv(int sockfd, void *buf, size_t len, int flags) {
  (void)flags;
  const int received = read(sockfd, (char *)buf, (int)len);
  if (received < 0) {
    errno = EWOULDBLOCK;
  }
  return (ssize_t)received;
}

extern "C" ssize_t opener_nb_sendto(int sockfd, const void *buf, size_t len, int flags,
                                    const struct sockaddr *dest_addr, socklen_t addrlen) {
  IPADDR4 dest_ip = IPADDR4::NullIP();
  uint16_t dest_port = 0U;

  (void)flags;

  if ((dest_addr != NULL) && (addrlen >= (socklen_t)sizeof(struct sockaddr_in))) {
    const struct sockaddr_in *sin = (const struct sockaddr_in *)dest_addr;
    dest_ip = ipaddr4_from_opener(sin->sin_addr.s_addr);
    dest_port = ntohs(sin->sin_port);
  }

  const int sent = sendto4(sockfd, (puint8_t)buf, (int)len, dest_ip, dest_port);
  if (sent < 0) {
    errno = EIO;
  }
  return (ssize_t)sent;
}

extern "C" ssize_t opener_nb_recvfrom(int sockfd, void *buf, size_t len, int flags,
                                      struct sockaddr *src_addr, socklen_t *addrlen) {
  IPADDR4 src_ip = IPADDR4::NullIP();
  uint16_t local_port = 0U;
  uint16_t remote_port = 0U;

  (void)flags;

  const int received = recvfrom4(sockfd, (puint8_t)buf, (int)len, &src_ip, &local_port, &remote_port);
  if (received < 0) {
    errno = EWOULDBLOCK;
    return -1;
  }

  if ((src_addr != NULL) && (addrlen != NULL) && (*addrlen >= (socklen_t)sizeof(struct sockaddr_in))) {
    struct sockaddr_in *sin = (struct sockaddr_in *)src_addr;
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(remote_port);
    sin->sin_addr.s_addr = (uint32_t)src_ip;
    *addrlen = (socklen_t)sizeof(struct sockaddr_in);
  }

  return (ssize_t)received;
}

extern "C" int opener_nb_setsockopt(int sockfd, int level, int optname,
                                    const void *optval, socklen_t optlen) {
  (void)sockfd;
  (void)level;
  (void)optname;
  (void)optval;
  (void)optlen;
  return 0;
}

extern "C" int opener_nb_getsockopt(int sockfd, int level, int optname,
                                    void *optval, socklen_t *optlen) {
  (void)sockfd;
  (void)level;
  (void)optname;
  (void)optval;
  (void)optlen;
  errno = ENOPROTOOPT;
  return -1;
}

extern "C" int opener_nb_shutdown(int sockfd, int how) {
  (void)how;
  return close(sockfd);
}

extern "C" int opener_nb_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  const NbPeerInfo *slot = peer_slot_for_fd(sockfd);

  if ((slot == NULL) || (slot->in_use == 0) || (addr == NULL) || (addrlen == NULL) ||
      (*addrlen < (socklen_t)sizeof(struct sockaddr_in))) {
    errno = ENOTCONN;
    return -1;
  }

  struct sockaddr_in *sin = (struct sockaddr_in *)addr;
  memset(sin, 0, sizeof(*sin));
  sin->sin_family = AF_INET;
  sin->sin_port = htons(slot->port);
  sin->sin_addr.s_addr = slot->addr_be;
  *addrlen = (socklen_t)sizeof(struct sockaddr_in);
  return 0;
}

extern "C" int opener_nb_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                                struct timeval *timeout) {
  unsigned long tick_timeout = 0UL;

  (void)writefds;
  (void)errorfds;

  if (timeout != NULL) {
    const unsigned long usec = (unsigned long)timeout->tv_sec * 1000000UL +
                               (unsigned long)timeout->tv_usec;
    tick_timeout = (usec * (unsigned long)TICKS_PER_SECOND) / 1000000UL;
    if ((usec > 0UL) && (tick_timeout == 0UL)) {
      tick_timeout = 1UL;
    }
  }

  return nb_sys_select(nfds, readfds, NULL, NULL, tick_timeout);
}
