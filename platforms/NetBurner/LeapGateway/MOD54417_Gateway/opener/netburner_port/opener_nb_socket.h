/*******************************************************************************
 * BSD-to-NetBurner socket shim declarations.
 ******************************************************************************/

#ifndef OPENER_NB_SOCKET_H_
#define OPENER_NB_SOCKET_H_

#include "opener_bsd_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

int opener_nb_socket(int domain, int type, int protocol);
int opener_nb_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int opener_nb_listen(int sockfd, int backlog);
int opener_nb_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int opener_nb_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t opener_nb_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t opener_nb_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t opener_nb_sendto(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t opener_nb_recvfrom(int sockfd, void *buf, size_t len, int flags,
                           struct sockaddr *src_addr, socklen_t *addrlen);
int opener_nb_setsockopt(int sockfd, int level, int optname,
                         const void *optval, socklen_t optlen);
int opener_nb_getsockopt(int sockfd, int level, int optname,
                         void *optval, socklen_t *optlen);
int opener_nb_shutdown(int sockfd, int how);
int opener_nb_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int nb_tcp_listen(uint16_t port, int backlog);
int nb_udp_listen(uint16_t port);
int nb_udp_io_socket(uint16_t port);

void nb_socket_store_peer(int sockfd, uint32_t peer_addr_be, uint16_t peer_port);

#ifdef __cplusplus
}
#endif

#endif /* OPENER_NB_SOCKET_H_ */
