#include "utils/error.h"

#include <stdio.h>
#include <string.h>

__thread int netc_errno_reason = 0;

char* netc_strerror()
{
#ifdef _WIN32
    char error[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error, 0, NULL);

    for (int i = 0; i < error; ++i)
    {
        if (error[i] == '\n')
        {
            if (error[i + 1] == '\0')
            {
                error[i] = '\0';
                break;
            }
            else error[i] = ' ';
        }
    }

    return error;
#else
    return strerror(errno);
#endif
};