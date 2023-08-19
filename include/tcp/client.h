#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "tcp/server.h"

extern __thread int netc_tcp_client_listening;

/** The main loop of a nonblocking TCP client. */
int tcp_client_main_loop(struct netc_tcp_client* client);

/** Initializes a TCP client. */
int tcp_client_init(struct netc_tcp_client* client, int ipv6, int non_blocking);
/** Connects the TCP client to the server. */
int tcp_client_connect(struct netc_tcp_client* client, struct sockaddr* address, socklen_t addrlen);
/** Sends data to the server. Returns the result of the `send` syscall. */
int tcp_client_send(struct netc_tcp_client* client, char* message, size_t msglen, int flags);
/** Receives data from the server. Returns the result of the `recv` syscall. */
int tcp_client_receive(struct netc_tcp_client* client, char* message, size_t msglen, int flags);

/** Closes the TCP client. */
int tcp_client_close(struct netc_tcp_client* client, int is_error);

#endif // TCP_CLIENT_H