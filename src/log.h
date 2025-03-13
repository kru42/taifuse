#ifndef _LOG_H_
#define _LOG_H_

#include <vitasdkkern.h>

// Define a static file descriptor for the log file
static SceUID log_fd = -1;

#define LOG(fmt, ...)                                                                                             \
    do                                                                                                            \
    {                                                                                                             \
        if (log_fd == -1)                                                                                         \
        {                                                                                                         \
            log_fd = ksceIoOpen("ux0:data/taifuse/taifuse.log", SCE_O_CREAT | SCE_O_WRONLY | SCE_O_APPEND, 0777); \
        }                                                                                                         \
        char buffer[256];                                                                                         \
        snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__);                                                     \
        ksceIoWrite(log_fd, buffer, strnlen(buffer, sizeof(buffer)));                                             \
        ksceIoWrite(log_fd, "\n", 1);                                                                             \
    } while (0)

#endif