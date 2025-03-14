#include <vitasdk.h>
#include <taihen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define printf sceClibPrintf

tai_hook_ref_t gxt_hook_ref;

int get_dr_module(tai_module_info_t* pt_module_info)
{
    pt_module_info->size = sizeof(tai_module_info_t);
    return taiGetModuleInfo("DR1_us", pt_module_info);
}

int loads_logo_gxt_hook(const char* a1, void* args)
{
    printf("loads_logo_gxt_hook: hello, called with \"%s\"...\n", a1);

    return TAI_CONTINUE(int, gxt_hook_ref, a1, args);
}

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    uint32_t func_addr = 0x810A9A96;

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