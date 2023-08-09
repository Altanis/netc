#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "utils/vector.h"
#include "socket.h"

#include <arpa/inet.h>

/** A structure representing a TCP client. */
struct netc_tcp_client
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The address of the server to connect to. */
    struct sockaddr sockaddr;
    /** The size of the client's address. */
    socklen_t addrlen;

    /** The polling file descriptor. */
    int pfd;

    /** The callback for when the client has connected to the server. */
    void (*on_connect)(struct netc_tcp_client* client);
    /** The callback for when the client has disconnected from the server. */
    void (*on_disconnect)(struct netc_tcp_client* client, int is_error);
    /** The callback for when the client has received a message from the server. */
    void (*on_data)(struct netc_tcp_client* client);
};

/** A structure representing the config of a TCP client. */
struct netc_tcp_client_config
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

/** A structure representing a TCP server. */
struct netc_tcp_server
{
    /** The socket file descriptor. */
    socket_t sockfd;
    /** The server's address. */
    struct sockaddr address;
    /** The size of the server's address. */
    socklen_t addrlen;
    /** The port the server is bound to. */
    int port;

    /** The maximum amount of sockets in the backlog. */
    int backlog;
    /** Whether or not all sockets should be non blocking. */
    int non_blocking;

    /** A vector of each client sockfd connected to the server. */
    struct vector* clients; // <socket_t>

    /** The polling file descriptor. */
    int pfd;

    /** The callback for when an incoming connection occurs. */
    void (*on_connect)(struct netc_tcp_server* server);
    /** The callback for when a message is received from a client. */
    void (*on_data)(struct netc_tcp_server* server, socket_t sockfd);
    /** The callback for when a client socket disconnects. */
    void (*on_disconnect)(struct netc_tcp_server* server, socket_t sockfd, int is_error);
};

/** A structure representing the config of a TCP server. */
struct netc_tcp_server_config
{
    /** The port the server is connected to. */
    int port;
    /** The maximum amount of sockets in the backlog. */
    int backlog;
    /** Whether or not to enable IPv6. */
    int ipv6;
    /** Whether or not to set the SO_REUSEADDR option to prevent EADDRINUSE. */
    int reuse_addr;

    /** Whether or not all sockets should be non blocking. */
    int non_blocking;
};

/** Whether or not the server is listening for events. */
extern __thread int netc_tcp_server_listening;

/** The main loop of a nonblocking TCP server. */
int tcp_server_main_loop(struct netc_tcp_server* server);

/** Initializes a TCP server. */
int tcp_server_init(struct netc_tcp_server* server, struct netc_tcp_server_config config);
/** Binds a TCP server to an address. */
int tcp_server_bind(struct netc_tcp_server* server);
/** Starts listening for connections on a TCP server. */
int tcp_server_listen(struct netc_tcp_server* server);
/** Accepts a connection on the TCP server. */
int tcp_server_accept(struct netc_tcp_server* server, struct netc_tcp_client* client);

/** Sends a message to the client. Returns a number less than 0 to represent the number of bytes left to send. */
int tcp_server_send(int sockfd, char* message, size_t msglen);
/** Receives a message from the client. Returns a number less than 0 to represent the number of bytes left to send. Returns 0 if the client has disconnected. */
int tcp_server_receive(int sockfd, char* message, size_t msglen);

/** Closes the TCP server. */
int tcp_server_close_self(struct netc_tcp_server* server);
/** Closes a client connection. */
int tcp_server_close_client(struct netc_tcp_server* server, int sockfd, int is_error);

#endif // TCP_SERVER_H