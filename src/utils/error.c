#include "../../include/utils/error.h"

#include <string.h>

__thread int netc_errno_reason = 0;

void netc_strerror(char *buffer)
{
#ifdef _WIN32
    char error[1024];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error,
        sizeof(error),
        NULL);
    
    for (size_t i = 0; i < sizeof(error); ++i)
    {
        if (error[i] == '\n')
        {
            if (error[i + 1] == '\0')
            {
                error[i] = '\0';
                break;
            } else error[i] = ' ';
        };
    };

    strcpy(buffer, error);
#else
    char error[1024] = {0};
    strerror_r(errno, error, sizeof(error) - 1);
    strcpy(buffer, error);
#endif
};

void netc_perror(const char *message)
{
    char error[1024] = {0};
    netc_strerror(error);

    fprintf(stderr, "%s: %s\n", message, error);
};