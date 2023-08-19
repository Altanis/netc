#include "utils/error.h"

#include <stdio.h>
#include <string.h>

__thread int netc_errno_reason = 0;

void netc_perror()
{
#ifdef _WIN32
    char* error = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error, 0, NULL);

    for (int i = 0; i < strlen(error); ++i)
        // formatmessage 🤡
        if (error[i] == '\n' || error[i] == '\r') error[i] = ' ';

    fprintf(stderr, "%s\n", error);
    LocalFree(error);
#else
    fprintf(stderr, "%s\n", strerror(errno));
#endif
};