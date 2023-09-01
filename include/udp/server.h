#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "utils/error.h"
#include "tcp/server.h"
#include "socket.h"

#include <arpa/inet.h>
#include <stdlib.h>

/** A structure representing a UDP server. */
struct udp_server
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The server's address. */
    struct sockaddr* sockaddr;
    /** The size of the server's address. */
    socklen_t addrlen;

    /** The polling file descriptor. */
    int pfd;

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** The callback for when data is received. */
    void (*on_data)(struct udp_server* server, void* data);
};

/** Whether or not the server is listening for events. */
extern __thread int netc_udp_server_listening;

/** The main loop of a nonblocking UDP server. */
int udp_server_main_loop(struct udp_server* server);

/** Initializes a UDP server. */
int udp_server_init(struct udp_server* server, int ipv6, int non_blocking);
/** Binds a UDP server to an address. */
int udp_server_bind(struct udp_server* server, struct sockaddr* addr, socklen_t addrlen);

/** Sends a message to a client. Returns the result of the `sendto` syscall. */
int udp_server_send(struct udp_server* server, char* message, size_t msglen, int flags, struct sockaddr* client_addr, socklen_t client_addrlen);
/** Receives a message from a client. Returns the result of the `recvfrom` syscall. */
int udp_server_receive(struct udp_server* server, char* message, size_t msglen, int flags, struct sockaddr* client_addr, socklen_t* client_addrlen);

/** Closes the UDP server. */
int udp_server_close(struct udp_server* server);

#endif // UDP_SERVER_H