#include "socket.h"

int socket_get_flags(socket_t sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);

    return flags;
};

int socket_set_non_blocking(socket_t sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return netc_error(FCNTL);
    else if (flags & O_NONBLOCK) return 0; // socket is already non-blocking

    int result = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) return netc_error(FCNTL);

    return 0;
};