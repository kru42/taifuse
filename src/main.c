#include <vitasdk.h>
#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Define a static file descriptor for the log file
static SceUID log_fd = -1;

#define LOG(fmt, ...)                                                                                     \
    do                                                                                                    \
    {                                                                                                     \
        if (log_fd == -1)                                                                                 \
        {                                                                                                 \
            log_fd = ksceIoOpen("ux0:data/taifuse.log", SCE_O_CREAT | SCE_O_WRONLY | SCE_O_APPEND, 0777); \
        }                                                                                                 \
        char buffer[256];                                                                                 \
        snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__);                                             \
        ksceIoWrite(log_fd, buffer, strnlen(buffer, sizeof(buffer)));                                     \
        ksceIoWrite(log_fd, "\n", 2);                                                                     \
    } while (0)

tai_hook_ref_t launch_app_by_name_ref;

int launch_app_by_name_hook(SceSize argc, const void* args)
{
    LOG("launch_app_by_name_hook called");

    return TAI_CONTINUE(int, launch_app_by_name_ref, argc, args);
}

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    LOG("hooking SceAppMgr__launchAppByName");

    taiHookFunctionExportForKernel(KERNEL_PID, &launch_app_by_name_ref, "SceAppMgr", TAI_ANY_LIBRARY, 0xC9C77E21,
                                   launch_app_by_name_hook);
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    taiHookReleaseForKernel(KERNEL_PID, launch_app_by_name_ref);
    return SCE_KERNEL_STOP_SUCCESS;
}
