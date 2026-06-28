/*******************************************************************************
 * Minimal socket type definitions for core compilation without NNDK headers.
 * NetBurner network I/O is implemented in source/src/ports/netburner/.
 ******************************************************************************/

#ifndef OPENER_SOCKADDR_H_
#define OPENER_SOCKADDR_H_

#include "typedefs.h"
#include "opener_inet.h"

struct in_addr {
  CipUdint s_addr;
};

struct sockaddr {
  uint16_t sa_family;
  char sa_data[14];
};

struct sockaddr_in {
  short sin_family;
  uint16_t sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};

#ifndef FD_SETSIZE
#define FD_SETSIZE 32
#endif

typedef struct {
  unsigned int fd_count;
  int fd_array[FD_SETSIZE];
} fd_set;

struct timeval {
  long tv_sec;
  long tv_usec;
};

typedef int socklen_t;

#endif /* OPENER_SOCKADDR_H_ */
