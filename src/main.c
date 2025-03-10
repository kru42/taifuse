#include <vitasdkkern.h>
#include <taihen.h>

#include "module.h"

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

typedef SceUID (*sceKernelGetProcessTitleId_t)(SceUID pid, char* titleid, int size);
static sceKernelGetProcessTitleId_t sceKernelGetProcessTitleId;

// static tai_hook_ref_t hook_ref;

// int sceKernelStartPreloadingModulesForKernel_hook(SceUID pid, void* args)
// {
//     LOG("sceKernelStartPreloadingModulesForKernel hook called!");
//     LOG("pid: %d\n", pid);

//     char titleid[32];
//     sceKernelGetProcessTitleId(pid, titleid, sizeof(titleid));
//     LOG("titleid: %s\n", titleid);

//     // Check if the titleid is a game
//     if (strncmp(titleid, "PC", 2) == 0)
//     {
//         LOG("Game detected!");

//         if (strcmp(titleid, "PCSA00092") == 0)
//         {
//             LOG("Soul Sacrifice detected, injecting code...");

//             void*   patch_addr = (void*)0x814F4478;
//             uint8_t patch_byte = 1;
//             taiInjectAbsForKernel(pid, patch_addr, &patch_byte, sizeof(patch_byte));
//             LOG("Code injected!");
//         }
//     }

//     return TAI_CONTINUE(int, hook_ref, pid, args);
// }

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    ksceIoMkdir("ux0:data/taifuse", 0777);

    tai_module_info_t module_info;
    if (taiGetModuleInfoForKernel(KERNEL_PID, "SceProcessmgr", &module_info) < 0)
    {
        LOG("Failed to get SceProcessmgr module info, aborting");
        return SCE_KERNEL_START_SUCCESS;
    }

    // if (module_get_export_func(KERNEL_PID, "SceProcessmgr", 0xEB1F8EF7, 0xEC3124A3, &sceKernelGetProcessTitleId) < 0)
    // {
    //     LOG("failed to find sceKernelGetProcessTitleId export in SceProcessmgr, aborting");
    //     return SCE_KERNEL_STOP_SUCCESS;
    // }

    // LOG("sceKernelGetProcessTitleId found!");

    // SceUID uid = taiHookFunctionExportForKernel(
    //     KERNEL_PID, &hook_ref, "SceKernelModulemgr", 0x92C9FFC2, 0x998C7AE9,
    //     sceKernelStartPreloadingModulesForKernel_hook); // SceModuleMgrForKernel ->
    //                                                     // sceKernelStartPreloadingModulesForKernel

    // if (uid < 0)
    //     LOG("failed to hook sceKernelStartPreloadingModulesForKernel: 0x%08x", uid);

    // LOG("hooked sceKernelStartPreloadingModulesForKernel");

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    // taiHookReleaseForKernel(KERNEL_PID, hook_ref);
    return SCE_KERNEL_STOP_SUCCESS;
}
