#ifndef SOCKET_H
#define SOCKET_H

#include "utils/error.h"
#include "utils/string.h"

#include <sys/types.h>
#include <sys/fcntl.h>

#ifdef _WIN32
    typedef int socklen_t;
    typedef SOCKET socket_t;
    typedef SSIZE_T ssize_t;
#else
    typedef int socket_t;
#endif

/** Dynamically receives from a socket until a certain byte pattern. */
ssize_t socket_recv_until_dynamic(socket_t sockfd, string_t* string, const char* bytes, int remove_delimiter, size_t max_bytes_received);
/** Receives from a socket until a certain byte pattern, or until a fixed length has been surpassed. */
ssize_t socket_recv_until_fixed(socket_t sockfd, char* buffer, size_t buffer_size, char* bytes, int remove_delimiter);

/** Gets a socket's flags. */
int socket_get_flags(socket_t sockfd);
/** Sets a socket to nonblocking mode. */
int socket_set_non_blocking(socket_t sockfd);

#endif // SOCKET_H