#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "tcp/server.h"

extern __thread int netc_tcp_client_listening;

/** The main loop of a nonblocking TCP client. */
int tcp_client_main_loop(struct netc_tcp_client* client);

/** Initializes a TCP client. */
int tcp_client_init(struct netc_tcp_client* client, struct netc_tcp_client_config config);
/** Connects the TCP client to the server. */
int tcp_client_connect(struct netc_tcp_client* client);
/** Sends data to the server. Returns -1 if message not sent in full. */
int tcp_client_send(struct netc_tcp_client* client, char* message, size_t msglen);
/** Receives data from the server. Returns -1 if message not received in full. */
int tcp_client_receive(struct netc_tcp_client* client, char* message, size_t msglen);

/** Closes the TCP client. */
int tcp_client_close(struct netc_tcp_client* client, int is_error);

#endif // TCP_CLIENT_H