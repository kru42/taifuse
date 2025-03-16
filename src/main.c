#include <vitasdkkern.h>
#include <taihen.h>

#include "log.h"
#include "cheats.h"
#include "plugin.h"
#include "menu.h"
#include "console.h"
#include "gui.h"
#include "hex_browser.h"
#include "taifuse.h"

volatile bool g_gui_updating   = false;
volatile int  g_gui_draw_frame = 0;
volatile int  g_gui_copy_frame = 0;

// Utility macro
#define DECL_FUNC_HOOK_PATCH_CTRL(index, name)                                         \
    static int name##_patched(int port, SceCtrlData* pad_data, int count)              \
    {                                                                                  \
        int ret = TAI_CONTINUE(int, g_ctrl_hook_refs[(index)], port, pad_data, count); \
        if (ret > 0)                                                                   \
            taifuse_input_check(pad_data, count);                                      \
        return ret;                                                                    \
    }

// taiHEN exports
extern int module_get_by_name_nid(SceUID pid, const char* name, uint32_t nid, tai_module_info_t* info);
extern int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t* addr);
extern int module_get_export_func(SceUID pid, const char* modname, uint32_t libnid, uint32_t funcnid, uintptr_t* func);
extern int module_get_import_func(SceUID pid, const char* modname, uint32_t target_libnid, uint32_t funcnid,
                                  uintptr_t* stub);

extern bool g_ui_state_changed;

typedef SceUID (*sceKernelGetProcessTitleId_t)(SceUID pid, char* titleid, int size);
typedef SceUID (*sceKernelUnloadProcessModulesForKernel_t)(SceUID pid);

static tai_hook_ref_t game_load_hook_ref;
static tai_hook_ref_t game_unload_hook_ref;
static tai_hook_ref_t g_display_fb_hook_ref;
tai_hook_ref_t        g_ctrl_hook_refs[8];

static sceKernelGetProcessTitleId_t sceKernelSysrootGetProcessTitleIdForKernel;

cheat_group_t* g_cheat_groups;
size_t         g_cheat_group_count;

SceUID g_taifuse_pool;
char   g_titleid[32];

SceUID g_game_pid;

static bool g_is_game_suspended = false;
bool        ui_needs_redraw     = true;

static int taifuse_thread(SceSize args, void* argp)
{
    // Get initial time in microseconds
    SceInt64 lastInputTime = ksceKernelGetSystemTimeWide();

    while (1)
    {
        // Wait for VBlank to sync rendering with display refresh.
        ksceDisplayWaitVblankStart();

        // Poll inputs only every 150ms (150,000 microseconds)
        SceInt64 currentTime = ksceKernelGetSystemTimeWide();
        if (currentTime - lastInputTime >= 150 * 1000)
        {
            SceCtrlData kctrl;
            int         ret = ksceCtrlPeekBufferPositive(0, &kctrl, 1);
            if (ret < 0)
                ret = ksceCtrlPeekBufferPositive(1, &kctrl, 1);
            if (ret > 0)
            {
                menu_handle_input(kctrl.buttons);
                console_handle_input(kctrl.buttons);
                // hex_browser_handle_input(kctrl.buttons);
            }
            lastInputTime = currentTime;
        }

        // Check for UI state or framebuffer resolution changes.
        bool state_changed = gui_state_changed();
        bool res_changed   = gui_fb_res_changed();
        if (state_changed || res_changed)
        {
            g_ui_state_changed = true;
        }

        // Begin updating GUI. This flag might be used to prevent concurrent writes.
        g_gui_updating = true;

        // If the UI state changed, clear and redraw static parts.
        if (g_ui_state_changed)
        {
            gui_clear();
            if (menu_is_active())
            {
                menu_draw_template();
            }
            else if (console_is_active())
            {
                console_draw_template();
            }
            // else if (hex_browser_is_active())
            // {
            //     hex_browser_draw_template();
            // }
            g_ui_state_changed = false;
        }

        // Draw dynamic (frequently changing) content continuously.
        if (menu_is_active())
        {
            menu_draw_dynamic();
        }
        else if (console_is_active())
        {
            console_draw_dynamic();
        }
        // else if (hex_browser_is_active())
        // {
        //     hex_browser_draw_dynamic();
        // }

        // Increment frame counter and release the updating lock.
        g_gui_draw_frame++;
        g_gui_updating = false;
    }
    return 0;
}

static void taifuse_input_check(SceCtrlData* pad_data, int count)
{
    if (menu_is_active() || console_is_active()) // || hex_browser_is_active())
    {
        SceCtrlData kctrl;
        kctrl.buttons = 0;
        for (int i = 0; i < count; i++)
            ksceKernelMemcpyKernelToUser((uintptr_t)&pad_data[i].buttons, &kctrl.buttons, sizeof(uint32_t));
    }
}

DECL_FUNC_HOOK_PATCH_CTRL(0, sceCtrlPeekBufferNegative)
DECL_FUNC_HOOK_PATCH_CTRL(1, sceCtrlPeekBufferNegative2)
DECL_FUNC_HOOK_PATCH_CTRL(2, sceCtrlPeekBufferPositive)
DECL_FUNC_HOOK_PATCH_CTRL(3, sceCtrlPeekBufferPositive2)
DECL_FUNC_HOOK_PATCH_CTRL(4, sceCtrlReadBufferNegative)
DECL_FUNC_HOOK_PATCH_CTRL(5, sceCtrlReadBufferNegative2)
DECL_FUNC_HOOK_PATCH_CTRL(6, sceCtrlReadBufferPositive)
DECL_FUNC_HOOK_PATCH_CTRL(7, sceCtrlReadBufferPositive2)

// Modify your ksceDisplaySetFrameBufInternal_patched function
int ksceDisplaySetFrameBufInternal_patched(int head, int index, const SceDisplayFrameBuf* pParam, int sync)
{
    if (head == 0 || pParam == NULL)
        goto DISPLAY_HOOK_RET;

    gui_set_framebuf(pParam);

    // Only copy if any UI is active and not currently updating
    if ((menu_is_active() || console_is_active() || hex_browser_is_active()) && !g_gui_updating &&
        g_gui_draw_frame > g_gui_copy_frame)
    {
        gui_cpy();
        g_gui_copy_frame = g_gui_draw_frame;
    }

DISPLAY_HOOK_RET:
    return TAI_CONTINUE(int, g_display_fb_hook_ref, head, index, pParam, sync);
}

int sceKernelStartPreloadingModulesForKernel_hook(SceUID pid, void* args)
{
    char titleid[32] = {0};
    sceKernelSysrootGetProcessTitleIdForKernel(pid, titleid, sizeof(titleid));

    int result = TAI_CONTINUE(int, game_load_hook_ref, pid, args);
    if (result < 0)
    {
        LOG("original sceKernelStartPreloadingModulesForKernel() failed, aborted");
        return result;
    }

    // Check if titleid starts with PC, if not bail out since we're not dealing with a game
    if (strncmp(titleid, "PC", 2) != 0)
    {
        return result;
    }

    g_game_pid = pid;
    memcpy(g_titleid, titleid, MAX_TITLEID_LEN);

    // TODO: redo cheats, and have patches instead of whatever cheats we have now,
    // since they're patches, not really cheats

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

int sceKernelUnloadProcessModulesForKernel_hook(SceUID pid)
{
    // Set game pid as zero, to indicate that we're not in a game anymore
    g_game_pid = 0;

    return TAI_CONTINUE(int, game_unload_hook_ref, pid);
}

void cleanup()
{
    // Uninstall hooks
    if (game_load_hook_ref)
        taiHookReleaseForKernel(KERNEL_PID, game_load_hook_ref);

    if (g_display_fb_hook_ref)
        taiHookReleaseForKernel(KERNEL_PID, g_display_fb_hook_ref);

    if (game_unload_hook_ref)
        taiHookReleaseForKernel(KERNEL_PID, game_unload_hook_ref);
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

    // Hook app/game modules preloading function
    SceUID res =
        taiHookFunctionExportForKernel(KERNEL_PID, &game_load_hook_ref, "SceKernelModulemgr", 0x92C9FFC2, 0x998C7AE9,
                                       sceKernelStartPreloadingModulesForKernel_hook); // SceModuleMgrForKernel ->
    if (res < 0)
    {
        LOG("failed to hook sceKernelStartPreloadingModulesForKernel: 0x%08x, aborted", res);
        return SCE_KERNEL_START_SUCCESS;
    }

    // Hook  app/game modules unloading function
    res = taiHookFunctionImportForKernel(KERNEL_PID, &game_unload_hook_ref, "SceProcessmgr", 0x92c9ffc2, 0xe71530d7,
                                         &sceKernelUnloadProcessModulesForKernel_hook);
    if (res < 0)
    {
        LOG("failed to hook sceProcessmgrUnloadProcessModulesForKernel: 0x%08x, aborted", res);
        cleanup();
        return SCE_KERNEL_START_SUCCESS;
    }

    // Init the display framebuffer first
    gui_init();

    // Init menu and console
    menu_init();
    console_init();
    hex_browser_init();

    res = taiHookFunctionExportForKernel(KERNEL_PID, &g_display_fb_hook_ref, "SceDisplay", 0x9FED47AC, 0x16466675,
                                         ksceDisplaySetFrameBufInternal_patched);
    if (res < 0)
    {
        LOG("failed to hook sceDisplaySetFrameBuf: 0x%08x, aborted", res);
        cleanup();
        return SCE_KERNEL_START_SUCCESS;
    }

    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[0], "SceCtrl", 0xD197E3C7, 0x104ED1A7,
                                   sceCtrlPeekBufferNegative_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[1], "SceCtrl", 0xD197E3C7, 0x81A89660,
                                   sceCtrlPeekBufferNegative2_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[2], "SceCtrl", 0xD197E3C7, 0xA9C3CED6,
                                   sceCtrlPeekBufferPositive_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[3], "SceCtrl", 0xD197E3C7, 0x15F81E8C,
                                   sceCtrlPeekBufferPositive2_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[4], "SceCtrl", 0xD197E3C7, 0x15F96FB0,
                                   sceCtrlReadBufferNegative_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[5], "SceCtrl", 0xD197E3C7, 0x27A0C5FB,
                                   sceCtrlReadBufferNegative2_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[6], "SceCtrl", 0xD197E3C7, 0x67E7AB83,
                                   sceCtrlReadBufferPositive_patched);
    taiHookFunctionExportForKernel(KERNEL_PID, &g_ctrl_hook_refs[7], "SceCtrl", 0xD197E3C7, 0xC4226A3E,
                                   sceCtrlReadBufferPositive2_patched);

    // Create main thread
    SceUID thread_uid = ksceKernelCreateThread("taifuse_thread", taifuse_thread, 0x3C, 0x3000, 0, 0x10000, 0);
    ksceKernelStartThread(thread_uid, 0, NULL);

    LOG("taifuse loaded, hooks installed");

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void* args)
{
    cleanup();
    return SCE_KERNEL_STOP_SUCCESS;
}
