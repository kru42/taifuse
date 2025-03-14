#include "taifuse.h"
#include "console.h"

void kuConsolePrintf(const char* format, ...)
{
    char    buffer[CONSOLE_MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    console_log("%s", buffer);
}
