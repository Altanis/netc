#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "udp/server.h"

extern __thread int netc_udp_client_listening;

/** A structure representing a UDP client. */
struct netc_udp_client
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The polling file descriptor. */
    int pfd;
    /** The client's address. */
    struct sockaddr_in sockaddr;
    /** The size of the client's address. */
    socklen_t addrlen;
    /** The port the client is bound to. */
    int port;

    /** The callback for when data is received. */
    void (*on_data)();
};

/** A structure representing the config of a UDP client. */
struct netc_udp_client_config
{
    /** Whether or not to connect from an IPv6 address. */
    int ipv6_connect_from;
    /** Whether or not to connect to an IPv6 address. */
    int ipv6_connect_to;

    /** The IP to connect to. */
    char* ip;
    /** The port the client is connected to. */
    int port;
    /** Whether or not the socket should be blocking. */
    int non_blocking;
};

/** Whether or not the client is listening for events. */
extern __thread int netc_udp_client_listening;

/** The main loop of a nonblocking UDP client. */
int udp_client_main_loop(struct netc_udp_client* client);

/** Initializes a UDP client. */
int udp_client_init(struct netc_udp_client* client, struct netc_udp_client_config config);
/** Receives a message from a server. */
int udp_client_receive(struct netc_udp_client* client, char* message, size_t msglen, struct sockaddr* server_addr, socklen_t* server_addrlen);
/** Sends a message to a server. */
int udp_client_send(struct netc_udp_client* client, const char* message, size_t msglen, struct sockaddr* server_addr, socklen_t server_addrlen);

#endif // UDP_CLIENT_H