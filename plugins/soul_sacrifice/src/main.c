#include <vitasdk.h>
#include <taihen.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define printf sceClibPrintf

int get_ss_module(tai_module_info_t* pt_module_info)
{
    pt_module_info->size = sizeof(tai_module_info_t);
    return taiGetModuleInfo("ef", pt_module_info);
}

void _start() __attribute__((weak, alias("module_start")));
int  module_start(SceSize argc, const void* args)
{
    // printf("soul_sacrifice_plugin: hello, doing hacks...\n");

    // tai_module_info_t pt_module_info;
    // int               res = get_ss_module(&pt_module_info);
    // if (res < 0)
    // {
    //     printf("soul_sacrifice_plugin: error getting module info via taihen!\n");
    //     return SCE_KERNEL_START_SUCCESS;
    // }

    // SceKernelModuleInfo module_info = {0};
    // if (sceKernelGetModuleInfo(pt_module_info.modid, &module_info) < 0)
    // {
    //     printf("soul_sacrifice_plugin: error getting module info via kernel!\n");
    //     return SCE_KERNEL_START_SUCCESS;
    // }

    // SceKernelSegmentInfo* seg0 = &(module_info.segments[0]);

    // printf("vaddr: %p\n", seg0->vaddr);
    // printf("memsz: 0x%08x\n", seg0->memsz);

    uint32_t debug_menu_patch_addr = 0x814F4478;
    uint8_t  debug_menu_patch_byte = 1;

    tai_module_info_t module_info;
    get_ss_module(&module_info);

    tai_offset_args_t offset_args;
    offset_args.size        = sizeof(offset_args);
    offset_args.modid       = module_info.modid;
    offset_args.segidx      = 0;
    offset_args.offset      = debug_menu_patch_addr - 0x81000000;
    offset_args.source      = &debug_menu_patch_byte;
    offset_args.source_size = sizeof(debug_menu_patch_byte);
    taiInjectDataForUser(&offset_args);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    return SCE_KERNEL_STOP_SUCCESS;
}