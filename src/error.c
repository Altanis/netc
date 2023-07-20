#include "error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    exit(EXIT_FAILURE);
};