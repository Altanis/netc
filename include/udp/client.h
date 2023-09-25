#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "udp/server.h"

/** A structure representing a UDP client. */
struct udp_client
{
    /** The socket file descriptor. */
    socket_t sockfd;
        /** The client's address. */
    struct sockaddr sockaddr;

    /** Whether or not the client is polling. */
    int listening;

#ifndef _WIN32
    /** The polling file descriptor. */
    int pfd;
#endif 

    /** User defined data to be passed to the event callbacks. */
    void *data;

    /** The callback for when data is received. */
    void (*on_data)(struct udp_client *client, void *data);
};

/** The main loop of a nonblocking UDP client. */
int udp_client_main_loop(struct udp_client *client);

/** Initializes a UDP client. */
int udp_client_init(struct udp_client *client, struct sockaddr addr, int non_blocking);
/** Connects a UDP client to a server. */
int udp_client_connect(struct udp_client *client);

/** Sends a message to a server. Returns the result of the `sendto` syscall. */
int udp_client_send(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t server_addrlen);
/** Receives a message from a server. Returns the result of the `recvfrom` syscall. */
int udp_client_receive(struct udp_client *client, char *message, size_t msglen, int flags, struct sockaddr *server_addr, socklen_t *server_addrlen);
/** Closes a UDP socket. */
int udp_client_close(struct udp_client *client);

#endif // UDP_CLIENT_H