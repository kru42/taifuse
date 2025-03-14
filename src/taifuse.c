#include "taifuse.h"
#include "console.h"

void kuConsolePrintf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    console_log(format, args);

    va_end(args);
}