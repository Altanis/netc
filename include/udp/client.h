#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "udp/server.h"

extern __thread int netc_udp_client_listening;

/** A structure representing a UDP client. */
struct netc_udp_client
{
#ifdef _WIN32
    /** The Windows socket library. */
    WSADATA* wsa;
#endif
    /** The socket file descriptor. */
    socket_t sockfd;
        /** The client's address. */
    struct sockaddr* sockaddr;
    /** The size of the client's address. */
    socklen_t addrlen;

    /** The polling file descriptor. */
    int pfd;

    /** User defined data to be passed to the event callbacks. */
    void* data;

    /** The callback for when data is received. */
    void (*on_data)(struct netc_udp_client* client, void* data);
};

/** Whether or not the client is listening for events. */
extern __thread int netc_udp_client_listening;

/** The main loop of a nonblocking UDP client. */
int udp_client_main_loop(struct netc_udp_client* client);

/** Initializes a UDP client. */
int udp_client_init(struct netc_udp_client* client, int ipv6, int non_blocking);
/** Connects a UDP client to a server. */
int udp_client_connect(struct netc_udp_client* client, struct sockaddr* addr, socklen_t addrlen);

/** Sends a message to a server. Returns the result of the `sendto` syscall. */
int udp_client_send(struct netc_udp_client* client, const char* message, size_t msglen, int flags, struct sockaddr* server_addr, socklen_t server_addrlen);
/** Receives a message from a server. Returns the result of the `recvfrom` syscall. */
int udp_client_receive(struct netc_udp_client* client, char* message, size_t msglen, int flags, struct sockaddr* server_addr, socklen_t* server_addrlen);
/** Closes a UDP socket. */
int udp_client_close(struct netc_udp_client* client);

#endif // UDP_CLIENT_H