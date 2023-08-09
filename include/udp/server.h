#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "utils/error.h"
#include "tcp/server.h"
#include "socket.h"

#include <arpa/inet.h>
#include <stdlib.h>

/** A structure representing a UDP server. */
struct netc_udp_server
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The polling file descriptor. */
    int pfd;
    /** The server's address. */
    struct sockaddr_in socket_addr;
    /** The size of the server's address. */
    socklen_t addrlen;
    /** The port the server is bound to. */
    int port;
    /** The maximum amount of sockets in the backlog. */
    int backlog;

    /** The callback for when data is received. */
    void (*on_data)();
};

/** A structure representing the config of a UDP server. */
struct netc_udp_server_config
{
    /** The port the server is bound to. */
    int port;
    /** Whether or not to enable IPv6. */
    int ipv6;
    /** Whether or not to set the SO_REUSEADDR option to prevent EADDRINUSE. */
    int reuse_addr;
    /** Whether or not the server is non blocking. */
    int non_blocking;
    /** The maximum amount of sockets in the backlog. */
    int backlog;
};

/** Whether or not the server is listening for events. */
extern __thread int netc_udp_server_listening;

/** The main loop of a nonblocking UDP server. */
int udp_server_main_loop(struct netc_udp_server* server);

/** Initializes a UDP server. */
int udp_server_init(struct netc_udp_server* server, struct netc_udp_server_config config);
/** Binds a UDP server to an address. */
int udp_server_bind(struct netc_udp_server* server);
/** Receives a message from a client. */
int udp_server_receive(struct netc_udp_server* server, char* message, size_t msglen, struct sockaddr* client_addr, socklen_t* client_addrlen);
/** Sends a message to a client. */
int udp_server_send(struct netc_udp_server* server, const char* message, size_t msglen, struct sockaddr* client_addr, socklen_t client_addrlen);

/** Closes the UDP server. */
int udp_server_close(struct netc_udp_server* server);

#endif // UDP_SERVER_H