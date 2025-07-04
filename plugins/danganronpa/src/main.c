#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <psp2/types.h>
#include <psp2/kernel/modulemgr.h>
#include <taihen.h>
#include "common.h"

tai_hook_ref_t gxt_hook_ref;

int get_dr_module(tai_module_info_t* pt_module_info)
{
    pt_module_info->size = sizeof(tai_module_info_t);
    return taiGetModuleInfo("DR1_us", pt_module_info);
}

int loads_logo_gxt_hook(const char* a1, void* args)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "loads_logo_gxt_hook: %s", a1);
    tf_console_print(buffer);
    return TAI_CONTINUE(int, gxt_hook_ref, a1, args);
}

void* start() __attribute__((weak, alias("module_start")));

int module_start(SceSize argc, const void* args)
{
    uint32_t func_addr = 0x810A9A96;

    char buffer[60];
    snprintf(buffer, sizeof(buffer), "hello hello uwu, hooking function at 0x%08x...", func_addr);
    tf_console_print(buffer);

    tai_module_info_t pt_module_info;
    int               res = get_dr_module(&pt_module_info);
    if (res < 0)
    {
        printf("danganronpa_plugin: error getting module info via taihen!\n");
        return SCE_KERNEL_START_SUCCESS;
    }

    tai_offset_args_t offset_args;
    offset_args.size        = sizeof(offset_args);
    offset_args.modid       = pt_module_info.modid;
    offset_args.segidx      = 0;
    offset_args.offset      = func_addr - 0x81000000;
    offset_args.source      = &loads_logo_gxt_hook;
    offset_args.source_size = sizeof(loads_logo_gxt_hook);

    res = taiHookFunctionOffsetForUser(&gxt_hook_ref, &offset_args);
    if (res < 0)
    {
        printf("Failed to hook function at address: %x\nret: %08x", func_addr, res);
        return SCE_KERNEL_START_SUCCESS;
    }

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    return SCE_KERNEL_STOP_SUCCESS;
}