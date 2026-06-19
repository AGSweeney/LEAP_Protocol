/*******************************************************************************
 * NetBurner platform network includes for OpENer.
 *
 * Maps BSD socket API names to NetBurner shim functions so generic_networkhandler.c
 * can compile unchanged (aside from OPENER_NETBURNER init hooks).
 ******************************************************************************/

#ifndef OPENER_NETBURNER_PLATFORM_NETWORK_INCLUDES_H_
#define OPENER_NETBURNER_PLATFORM_NETWORK_INCLUDES_H_

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <constants.h>

#include "opener_bsd_compat.h"

#ifndef _NB_IOSYS_H
typedef struct {
  uint32_t fd_set_elements[FDSET_ELEMENTS];
} fd_set;

void FD_ZERO(fd_set *pfds);
void FD_CLR(int fd, fd_set *pfds);
void FD_SET(int fd, fd_set *pfds);
int FD_ISSET(int fd, fd_set *pfds);
int nb_sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, unsigned long timeout);
#endif
int opener_nb_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
                     struct timeval *timeout);

#include "opener_nb_socket.h"

#define select opener_nb_select
#define socket opener_nb_socket
#define bind opener_nb_bind
#define listen opener_nb_listen
#define accept opener_nb_accept
#define connect opener_nb_connect
#define send opener_nb_send
#define recv opener_nb_recv
#define sendto opener_nb_sendto
#define recvfrom opener_nb_recvfrom
#define setsockopt opener_nb_setsockopt
#define getsockopt opener_nb_getsockopt
#define shutdown opener_nb_shutdown
#define getpeername opener_nb_getpeername

#endif /* OPENER_NETBURNER_PLATFORM_NETWORK_INCLUDES_H_ */
