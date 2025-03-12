#include <vitasdkkern.h>
#include <taihen.h>

// taiHEN exports
extern int module_get_by_name_nid(SceUID pid, const char* name, uint32_t nid, tai_module_info_t* info);
extern int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t* addr);
extern int module_get_export_func(SceUID pid, const char* modname, uint32_t libnid, uint32_t funcnid, uintptr_t* func);
extern int module_get_import_func(SceUID pid, const char* modname, uint32_t target_libnid, uint32_t funcnid,
                                  uintptr_t* stub);

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
static sceKernelGetProcessTitleId_t sceKernelSysrootGetProcessTitleIdForKernel;

typedef SceUID (*sceKernelUnloadProcessModulesForKernel_t)(SceUID pid);
static sceKernelUnloadProcessModulesForKernel_t sceKernelUnloadProcessModulesForKernel;

static tai_hook_ref_t game_load_hook_ref;
static tai_hook_ref_t dr_hook_ref;

void hook_debug_print_nullsub(int a1, int a2, const char* msg)
{
    LOG("hook_debug_print_nullsub called!");
    LOG("%s", msg);
    return TAI_CONTINUE(void, dr_hook_ref, a1, a2, msg);
}

int sceKernelStartPreloadingModulesForKernel_hook(SceUID pid, void* args)
{
    LOG("sceKernelStartPreloadingModulesForKernel hook called!");
    LOG("pid: %d", pid);

    char titleid[32];
    sceKernelSysrootGetProcessTitleIdForKernel(pid, titleid, sizeof(titleid));
    LOG("titleid: %s", titleid);

    // Check if the titleid is a game
    if (strncmp(titleid, "PC", 2) == 0)
    {
        LOG("Game detected!");

        if (strcmp(titleid, "PCSA00092") == 0)
        {
            LOG("Soul Sacrifice detected, injecting code...");

            void*   patch_addr = (void*)0x814F4478;
            uint8_t patch_byte = 1;
            taiInjectAbsForKernel(pid, patch_addr, &patch_byte, sizeof(patch_byte));
            LOG("Code injected!");
        }
        else if (strcmp(titleid, "PCSE00261") == 0)
        {
            LOG("Danganronpa detected, hooking debug print nullsub...");

            void* hook_addr = (void*)0x81091D44;
            void* hook_func = (void*)hook_debug_print_nullsub;

            taiHookFunctionAbs(pid, &dr_hook_ref, hook_addr, hook_func);
            LOG("Debug print nullsub hooked!");
        }
    }

    return TAI_CONTINUE(int, game_load_hook_ref, pid, args);
}

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    ksceIoMkdir("ux0:data/taifuse", 0777);

    if (module_get_export_func(KERNEL_PID, "SceSysmem", TAI_ANY_LIBRARY, 0xEC3124A3,
                               &sceKernelSysrootGetProcessTitleIdForKernel) < 0)
    {
        LOG("failed to find sceKernelSysrootGetProcessTitleIdForKernel export in SceSysmem, aborted");
        return SCE_KERNEL_STOP_SUCCESS;
    }

    LOG("sceKernelSysrootGetProcessTitleIdForKernel found!");
    LOG("sceKernelSysrootGetProcessTitleIdForKernel address: %p", sceKernelSysrootGetProcessTitleIdForKernel);

    SceUID uid = taiHookFunctionExportForKernel(
        KERNEL_PID, &game_load_hook_ref, "SceKernelModulemgr", 0x92C9FFC2, 0x998C7AE9,
        sceKernelStartPreloadingModulesForKernel_hook); // SceModuleMgrForKernel ->
                                                        // sceKernelStartPreloadingModulesForKernel

    // TODO: hook unload (named same but unloading on vita wiki), and uninstall
    // game hooks etc..
    0xE71530D7

        if (uid < 0) LOG("failed to hook sceKernelStartPreloadingModulesForKernel: 0x%08x", uid);

    LOG("hooked sceKernelStartPreloadingModulesForKernel");

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    taiHookReleaseForKernel(KERNEL_PID, game_load_hook_ref);
    return SCE_KERNEL_STOP_SUCCESS;
}
