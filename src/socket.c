#include "socket.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

ssize_t socket_recv_until_dynamic(socket_t sockfd, string_t* string, const char* bytes, int remove_delimiter, size_t max_bytes_received)
{
    size_t bytes_len = strlen(bytes);
    size_t bytes_received = 0;

    while (bytes_received <= max_bytes_received)
    {
        char c;
        ssize_t recv_result = recv(sockfd, &c, sizeof c, 0);
        if (recv_result <= 0)
        {
            if (recv_result == -1) 
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                netc_error(BADRECV);
            };

            return recv_result;
        };

        bytes_received++;
        sso_string_concat_char(string, c);

        if (bytes_received >= bytes_len && strcmp(sso_string_get(string) + string->length - bytes_len, bytes) == 0)
        {
            if (remove_delimiter)
            {
                sso_string_backspace(string, bytes_len);
                bytes_received -= bytes_len;
            };

            break;
        };
    };

    return bytes_received;
};

ssize_t socket_recv_until_fixed(socket_t sockfd, char* buffer, size_t buffer_size, char* bytes, int remove_delimiter)
{
    size_t bytes_len = strlen(bytes);
    size_t bytes_received = 0;

    while ((bytes_received + bytes_len) <= buffer_size)
    {
        ssize_t recv_result = recv(sockfd, buffer + bytes_received, 1, 0);
        if (recv_result <= 0) 
        {
            if (recv_result == -1) 
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                netc_error(BADRECV);
            };

            return recv_result;
        };

        bytes_received += recv_result;

        if (bytes_received >= bytes_len && strncmp(buffer + bytes_received - bytes_len, bytes, bytes_len) == 0)
        {
            if (remove_delimiter)
            {
                buffer[bytes_received - bytes_len] = '\0';
                --bytes_received;
            };

            break;
        }
    };

    return bytes_received;
};

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