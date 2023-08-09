#ifndef SOCKET_H
#define SOCKET_H

#include "utils/error.h"

#include <sys/fcntl.h>

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

/** Gets a socket's flags. */
int socket_get_flags(socket_t sockfd);
/** Sets a socket to nonblocking mode. */
int socket_set_non_blocking(socket_t sockfd);

#endif // SOCKET_H