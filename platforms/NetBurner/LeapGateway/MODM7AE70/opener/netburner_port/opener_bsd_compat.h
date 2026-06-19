/*******************************************************************************
 * Minimal BSD socket definitions for OpENer on NetBurner.
 ******************************************************************************/

#ifndef OPENER_BSD_COMPAT_H_
#define OPENER_BSD_COMPAT_H_

#include <stdint.h>
#include <stddef.h>

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif

#ifndef SOL_SOCKET
#define SOL_SOCKET 1
#endif

#ifndef SO_REUSEADDR
#define SO_REUSEADDR 2
#endif

#ifndef SO_BROADCAST
#define SO_BROADCAST 6
#endif

#ifndef IP_TOS
#define IP_TOS 1
#endif

#ifndef IP_MULTICAST_TTL
#define IP_MULTICAST_TTL 2
#endif

#ifndef IP_MULTICAST_IF
#define IP_MULTICAST_IF 3
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef INADDR_ANY
#define INADDR_ANY ((uint32_t)0U)
#endif

#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((uint32_t)0xFFFFFFFFUL)
#endif

typedef uint32_t in_addr_t;

#ifndef IN_CLASSA
#define IN_CLASSA(i) (((uint32_t)(i) & 0x80000000UL) == 0U)
#endif

#ifndef IN_CLASSB
#define IN_CLASSB(i) (((uint32_t)(i) & 0xc0000000UL) == 0x80000000UL)
#endif

#ifndef IN_CLASSC
#define IN_CLASSC(i) (((uint32_t)(i) & 0xe0000000UL) == 0xc0000000UL)
#endif

#ifndef IN_CLASSA_NET
#define IN_CLASSA_NET 0xff000000UL
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001UL)
#endif

#ifndef IF_NAMESIZE
#define IF_NAMESIZE 32
#endif

typedef uint32_t socklen_t;
typedef uint16_t in_port_t;

struct in_addr {
  uint32_t s_addr;
};

struct sockaddr {
  uint16_t sa_family;
  char sa_data[14];
};

struct sockaddr_in {
  uint16_t sin_family;
  uint16_t sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};

#ifndef _SYS__TIMEVAL_H_
struct timeval {
  long tv_sec;
  long tv_usec;
};
#endif

#if defined(COLDFIRE) || defined(MCF5441X) || defined(MOD5441X) || defined(__mcoldfire__)
#define OPENER_NB_HOST_BIG_ENDIAN 1
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define OPENER_NB_HOST_BIG_ENDIAN 1
#else
#define OPENER_NB_HOST_BIG_ENDIAN 0
#endif

static inline uint16_t opener_htons(uint16_t hostshort) {
#if OPENER_NB_HOST_BIG_ENDIAN
  return hostshort;
#else
  return (uint16_t)(((hostshort & 0xFFU) << 8) | ((hostshort >> 8) & 0xFFU));
#endif
}

static inline uint16_t opener_ntohs(uint16_t netshort) {
  return opener_htons(netshort);
}

static inline uint32_t opener_htonl(uint32_t hostlong) {
#if OPENER_NB_HOST_BIG_ENDIAN
  return hostlong;
#else
  return ((hostlong & 0x000000FFUL) << 24) |
         ((hostlong & 0x0000FF00UL) << 8) |
         ((hostlong & 0x00FF0000UL) >> 8) |
         ((hostlong & 0xFF000000UL) >> 24);
#endif
}

static inline uint32_t opener_ntohl(uint32_t netlong) {
  return opener_htonl(netlong);
}

#ifndef htons
#define htons opener_htons
#endif

#ifndef ntohs
#define ntohs opener_ntohs
#endif

#ifndef htonl
#define htonl opener_htonl
#endif

#ifndef ntohl
#define ntohl opener_ntohl
#endif

#ifndef ntohl
#define ntohl opener_ntohl
#endif

#if OPENER_NB_HOST_BIG_ENDIAN
static inline uint32_t opener_inet_addr(const char *cp) {
  uint32_t parts[4] = {0U, 0U, 0U, 0U};
  int part = 0;
  uint32_t value = 0U;

  if (cp == NULL) {
    return (uint32_t)-1;
  }

  for (; *cp != '\0'; ++cp) {
    if (*cp == '.') {
      if (part > 3) {
        return (uint32_t)-1;
      }
      parts[part++] = value;
      value = 0U;
    } else if ((*cp >= '0') && (*cp <= '9')) {
      value = (value * 10U) + (uint32_t)(*cp - '0');
      if (value > 255U) {
        return (uint32_t)-1;
      }
    } else {
      return (uint32_t)-1;
    }
  }

  if (part != 3) {
    return (uint32_t)-1;
  }
  parts[3] = value;

  return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}
#else
static inline uint32_t opener_inet_addr(const char *cp) {
  uint32_t parts[4] = {0U, 0U, 0U, 0U};
  int part = 0;
  uint32_t value = 0U;

  if (cp == NULL) {
    return (uint32_t)-1;
  }

  for (; *cp != '\0'; ++cp) {
    if (*cp == '.') {
      if (part > 3) {
        return (uint32_t)-1;
      }
      parts[part++] = value;
      value = 0U;
    } else if ((*cp >= '0') && (*cp <= '9')) {
      value = (value * 10U) + (uint32_t)(*cp - '0');
      if (value > 255U) {
        return (uint32_t)-1;
      }
    } else {
      return (uint32_t)-1;
    }
  }

  if (part != 3) {
    return (uint32_t)-1;
  }
  parts[3] = value;

  return htonl((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]);
}
#endif

#ifndef inet_addr
#define inet_addr opener_inet_addr
#endif

#endif /* OPENER_BSD_COMPAT_H_ */
