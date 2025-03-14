#include <vitasdkkern.h>
#include <taihen.h>

#include "log.h"
#include "cheats.h"
#include "plugin.h"
#include "gui.h"

// taiHEN exports
extern int module_get_by_name_nid(SceUID pid, const char* name, uint32_t nid, tai_module_info_t* info);
extern int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t* addr);
extern int module_get_export_func(SceUID pid, const char* modname, uint32_t libnid, uint32_t funcnid, uintptr_t* func);
extern int module_get_import_func(SceUID pid, const char* modname, uint32_t target_libnid, uint32_t funcnid,
                                  uintptr_t* stub);

typedef SceUID (*sceKernelGetProcessTitleId_t)(SceUID pid, char* titleid, int size);
typedef SceUID (*sceKernelUnloadProcessModulesForKernel_t)(SceUID pid);

static tai_hook_ref_t game_load_hook_ref;

static sceKernelGetProcessTitleId_t             sceKernelSysrootGetProcessTitleIdForKernel;
static sceKernelUnloadProcessModulesForKernel_t sceKernelUnloadProcessModulesForKernel;

static cheat_group_t* g_cheat_groups;
static size_t         g_cheat_group_count;

SceUID g_taifuse_pool;

int sceKernelStartPreloadingModulesForKernel_hook(SceUID pid, void* args)
{
    char titleid[32] = {0};
    sceKernelSysrootGetProcessTitleIdForKernel(pid, titleid, sizeof(titleid));

    LOG("%s is running with pid %d", titleid, pid);

    LOG("calling original sceKernelStartPreloadingModulesForKernel()...");
    int result = TAI_CONTINUE(int, game_load_hook_ref, pid, args);
    if (result < 0)
    {
        LOG("original sceKernelStartPreloadingModulesForKernel() failed, aborted");
        return result;
    }

    // Reload the cheat file on every game load.
    // Reset our static cheat group count.
    g_cheat_group_count = 0;
    if (load_cheats("ux0:data/taifuse/cheats.ini", &g_cheat_groups, &g_cheat_group_count) < 0)
    {
        LOG("Failed to load cheats for %s", titleid);
    }
    else
    {
        // This function will scan all groups for a matching titleid.
        apply_cheats(pid, titleid, g_cheat_groups, g_cheat_group_count);
    }

    load_plugins_for_game(pid, titleid);

    return result;
}

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    ksceIoMkdir("ux0:data/taifuse", 0777);

    SceKernelHeapCreateOpt opt;
    memset(&opt, 0x00, sizeof(opt));
    opt.size    = sizeof(opt);
    opt.uselock = 1;

    g_taifuse_pool = ksceKernelCreateHeap("taifuse_heap", 0x8000, &opt);
    if (g_taifuse_pool < 0)
    {
        LOG("failed to create pool, aborted");
        return SCE_KERNEL_STOP_SUCCESS;
    }

    if (module_get_export_func(KERNEL_PID, "SceSysmem", TAI_ANY_LIBRARY, 0xEC3124A3,
                               &sceKernelSysrootGetProcessTitleIdForKernel) < 0)
    {
        LOG("failed to find sceKernelSysrootGetProcessTitleIdForKernel export in SceSysmem, aborted");
        return SCE_KERNEL_STOP_SUCCESS;
    }

    SceUID uid = taiHookFunctionExportForKernel(
        KERNEL_PID, &game_load_hook_ref, "SceKernelModulemgr", 0x92C9FFC2, 0x998C7AE9,
        sceKernelStartPreloadingModulesForKernel_hook); // SceModuleMgrForKernel ->
                                                        // sceKernelStartPreloadingModulesForKernel

    // TODO: hook unload (reverse vitacheat lol to figure out what function), and uninstall
    // game hooks etc..
    if (uid < 0)
    {
        LOG("failed to hook sceKernelStartPreloadingModulesForKernel: 0x%08x, aborted", uid);
    }

    // SceUID thread_uid = ksceKernelCreateThread("taifuse_thread", taifuse_thread, 0x3C, 0x3000, 0, 0x10000, 0);
    // ksceKernelStartThread(thread_uid, 0, NULL);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    taiHookReleaseForKernel(KERNEL_PID, game_load_hook_ref);
    return SCE_KERNEL_STOP_SUCCESS;
}
