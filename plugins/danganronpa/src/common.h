#ifndef COMMON_H_
#define COMMON_H_

// Now include the standard SDK headers
#include <vitasdk.h>
#include <stdio.h>
#include <taifuse.h>

// #include <kubridge.h>

#define printf sceClibPrintf
// #define console_printf(fmt, ...)                                                                                    \
    do                                                                                                              \
    {                                                                                                               \
        char user_buffer[256];                                                                                      \
        snprintf(user_buffer, sizeof(user_buffer), fmt, __VA_ARGS__);                                               \
                                                                                                                    \
        void*  kernel_buffer = NULL;                                                                                \
        SceUID mem_id        = kuKernelAllocMemBlock("taifuse_console", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_GAME_RW, \
                                                     sizeof(user_buffer), 0);                                       \
        if (mem_id >= 0)                                                                                            \
        {                                                                                                           \
            int ret = sceKernelGetMemBlockBase(mem_id, &kernel_buffer);                                             \
            if (ret >= 0 && kernel_buffer != NULL)                                                                  \
            {                                                                                                       \
                kuKernelCpuUnrestrictedMemcpy(kernel_buffer, user_buffer, sizeof(user_buffer));                     \
                kuConsolePrintf("%s", kernel_buffer);                                                               \
                sceKernelFreeMemBlock(mem_id);                                                                      \
            }                                                                                                       \
            else                                                                                                    \
            {                                                                                                       \
                printf("Failed to get memory block base: 0x%08x\n", ret);                                           \
                sceKernelFreeMemBlock(mem_id);                                                                      \
            }                                                                                                       \
        }                                                                                                           \
        else                                                                                                        \
        {                                                                                                           \
            printf("Failed to allocate kernel memory for the buffer: 0x%08x\n", mem_id);                            \
        }                                                                                                           \
    } while (0)

#endif // COMMON_H_