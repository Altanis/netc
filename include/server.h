#ifndef SERVER_H
#define SERVER_H

#include "vector.h"

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#endif

/** A structure representing the client. */
struct netc_client_t
{
    /** The socket file descriptor. */
    int socket_fd;
    /** The address of the server to connect to. */
    struct sockaddr address;
    /** The size of the client's address. */
    int addrlen;

    /** The callback for when the client has connected to the server. */
    void (*on_connect)(struct netc_client_t* client);
    /** The callback for when the client has disconnected from the server. */
    void (*on_disconnect)(struct netc_client_t* client);
    /** The callback for when the client has received a message from the server. */
    void (*on_data)(struct netc_client_t* client, char* message);
};

/** A structure representing the server. */
struct netc_server_t
{
    /** The socket file descriptor. */
    int socket_fd;
    /** The server's address. */
    struct sockaddr address;
    /** The size of the server's address. */
    int addrlen;

    /** The port the server is running on. */
    int port;
    /** The maximum amount of connections permitted by the server. */
    int max_connections;
    /** The maximum amount of sockets in the backlog. */
    int backlog;

    /** A vector of each client struct connected to the server. */
    struct vector* clients;
    /** The last client which connected to the server. */
    struct netc_client_t* last_client;

    /** The file descriptor representing the current polling file descriptor. */
    int pfd; 

    /** The callback for when the server is ready to listen for connections. */
    void (*on_ready)(struct netc_server_t* server);
    /** The callback for when an incoming connection occurs. */
    void (*on_connect)(struct netc_server_t* server);
    /** The callback for when a client socket disconnects. */
    void (*on_disconnect)(struct netc_server_t* server, struct netc_client_t* client, int is_error);
    /** The callback for when a message is received from a client. */
    void (*on_data)(struct netc_server_t* server, struct netc_client_t* client);
};

/** A structure representing the config of a client. */
struct netc_client_config
{
    /** Whether or not to enable IPv6. */
    int ipv6;
    /** The port the client is connected to. */
    int port;
};

/** A structure representing the config of the server. */
struct netc_server_config
{
    /** The port the server is connected to. */
    int port;
    /** The maximum amount of sockets in the backlog. */
    int backlog;
    /** Whether or not to enable IPv6. */
    int ipv6;
    /** Whether or not to set the SO_REUSEADDR option to prevent EADDRINUSE. */
    int reuse_addr;

    /** Whether or not to set sockets to non blocking mode. */
    int non_blocking;
};

/** Whether or not the server is running. */
int is_running;

/** Initializes the server. */
int server_init(struct netc_server_t* server, struct netc_server_config config);

/** Binds the server to the socket. */
int server_bind(struct netc_server_t* server);
/** Listens for connections on the server. */
int server_listen(struct netc_server_t* server);
/** Accepts a connection on the server. */
int server_accept(struct netc_server_t* server, struct netc_client_t* client);
/** The main loop of the server. */
int server_main_loop(struct netc_server_t* server);

struct netc_client_t* server_get_client(struct netc_server_t* server, int sockfd);

/** Sends a message to the client. Returns a number less than 0 to represent the number of bytes left to send. */
int server_send(struct netc_client_t* client, char* message, size_t msglen);
/** Receives a message from the client. Returns a number less than 0 to represent the number of bytes left to send. */
int server_receive(struct netc_server_t* server, struct netc_client_t* client, char* message, size_t msglen);

/** Closes the server. */
int server_close_self(struct netc_server_t* server);
/** Closes a client connection. */
int server_close_client(struct netc_server_t* server, struct netc_client_t* client, int is_error);

/** Gets the client's flags. */
int socket_get_flags(int sockfd);
/** Sets a client to nonblocking mode. */
int socket_set_non_blocking(int sockfd);

#endif // SERVER_H