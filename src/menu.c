#include "menu.h"
#include <taihen.h>
#include <stdio.h>

#include "gui.h"
#include "cheats.h"
#include "console.h"
#include "log.h"

extern int module_get_export_func(SceUID pid, const char* modname, uint32_t libnid, uint32_t funcnid, uintptr_t* func);

// option: 0: flag 0x2, 1: flag 0x20, 2: flag 0x40
typedef int (*sceKernelKillProcessForKernel_t)(SceUID pid, SceInt32 option);
static sceKernelKillProcessForKernel_t sceKernelKillProcessForKernel;

extern int            g_game_pid;
extern char           g_titleid[32];
extern rgba_t         g_color_text;
extern cheat_group_t* g_cheat_groups;
extern size_t         g_cheat_group_count;

// Menu-specific internal state.
static bool g_menu_active     = false;
static int  g_selected_option = 0;

// Define this variable as it's referenced externally
bool g_last_menu_active = false;

// Toggle the menu's active state.
void menu_toggle(void)
{
    g_menu_active = !g_menu_active;
}

// Returns true if the menu is active.
bool menu_is_active(void)
{
    return g_menu_active;
}

void menu_handle_input(SceCtrlButtons buttons)
{
    static bool combo_pressed = false;

    // Use static variables to store previous button states for edge detection.
    static bool up_was_pressed   = false;
    static bool down_was_pressed = false;
    // And static timers for auto-repeat.
    static SceInt64 up_last_time   = 0;
    static SceInt64 down_last_time = 0;

    // Delays in microseconds.
    const SceInt64 initialDelay = 300 * 1000; // 300ms before auto-repeat starts.
    const SceInt64 repeatDelay  = 150 * 1000; // 150ms between repeats.

    SceInt64 currentTime = ksceKernelGetSystemTimeWide();

    // Check for the toggle combo.
    bool toggle_pressed = (buttons & SCE_CTRL_LTRIGGER) && (buttons & SCE_CTRL_RTRIGGER) && (buttons & SCE_CTRL_UP);
    if (toggle_pressed && !combo_pressed)
    {
        menu_toggle();
        combo_pressed = true;
    }
    else if (!toggle_pressed && combo_pressed)
    {
        combo_pressed = false;
    }

    // Process navigation only when menu is active.
    if (g_menu_active)
    {
        // UP button handling.
        if (buttons & SCE_CTRL_UP)
        {
            if (!up_was_pressed)
            {
                // Immediate reaction on rising edge.
                g_selected_option--;
                up_last_time   = currentTime;
                up_was_pressed = true;
            }
            else if (currentTime - up_last_time >= (up_last_time == currentTime ? initialDelay : repeatDelay))
            {
                // Auto-repeat: if held long enough after the initial press.
                g_selected_option--;
                up_last_time = currentTime;
            }
        }
        else
        {
            up_was_pressed = false;
        }

        // DOWN button handling.
        if (buttons & SCE_CTRL_DOWN)
        {
            if (!down_was_pressed)
            {
                // Immediate reaction on rising edge.
                g_selected_option++;
                down_last_time   = currentTime;
                down_was_pressed = true;
            }
            else if (currentTime - down_last_time >= (down_last_time == currentTime ? initialDelay : repeatDelay))
            {
                // Auto-repeat: if held long enough after the initial press.
                g_selected_option++;
                down_last_time = currentTime;
            }
        }
        else
        {
            down_was_pressed = false;
        }

        // Wrap around the selection.
        const int option_count = (g_game_pid != NULL) ? 3 : 2;
        if (g_selected_option < 0)
            g_selected_option = option_count - 1;
        else if (g_selected_option >= option_count)
            g_selected_option = 0;

        // Process confirmation.
        if (buttons & SCE_CTRL_CIRCLE)
        {
            if (g_game_pid != NULL)
            {
                if (g_selected_option == 0)
                {
                    free_cheats(g_cheat_groups, g_cheat_group_count);
                    if (load_cheats("ux0:data/taifuse/cheats.ini", &g_cheat_groups, &g_cheat_group_count) == 0)
                    {
                        set_cheats_enabled_for_game(g_cheat_groups, g_cheat_group_count, g_titleid, true);
                    }
                }
                else if (g_selected_option == 1)
                {
                    if (sceKernelKillProcessForKernel == NULL)
                    {
                        if (module_get_export_func(KERNEL_PID, "SceProcessmgr", TAI_ANY_LIBRARY, 0xF388F05C,
                                                   &sceKernelKillProcessForKernel) < 0)
                        {
                            LOG("Failed to find SceProcessmgr::sceKernelKillProcessForKernel, operation aborted");
                            return;
                        }
                    }
                    sceKernelKillProcessForKernel(g_game_pid, 0x2);
                }
                else if (g_selected_option == 2)
                {
                    g_menu_active = false;
                }
            }
            else // g_game_pid == NULL case.
            {
                if (g_selected_option == 0)
                {
                    kscePowerRequestSoftReset();
                }
                else if (g_selected_option == 1)
                {
                    kscePowerRequestColdReset();
                }
            }
        }
    }
}

// Draw the static parts of the menu (template)
void menu_draw_template(void)
{
    if (!menu_is_active())
        return;

    // Save the original text color.
    rgba_t original_color = g_color_text;

    // Ensure title is drawn in white.
    g_color_text.rgba.r = 255;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 255;
    gui_print(50, 30, "Taifuse menu");

    // Restore original color in case it's used elsewhere.
    g_color_text = original_color;
}

// Draw the dynamic parts of the menu that change with user interaction
void menu_draw_dynamic(void)
{
    if (!menu_is_active())
        return;

    // Save the original text color.
    rgba_t original_color = g_color_text;

    if (g_game_pid != 0)
    {
        char reload_option[64];
        snprintf(reload_option, sizeof(reload_option), "Reload cheats for %s", g_titleid);

        const char* options[]    = {reload_option, "Kill game process", "Exit"};
        const int   option_count = sizeof(options) / sizeof(options[0]);

        for (int i = 0; i < option_count; i++)
        {
            char option_text[64];
            if (i == g_selected_option)
            {
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 0;
                g_color_text.rgba.b = 0;

                snprintf(option_text, sizeof(option_text), "> %s", options[i]);
            }
            else
            {
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 255;
                g_color_text.rgba.b = 255;

                snprintf(option_text, sizeof(option_text), "  %s", options[i]);
            }

            gui_print(60, 60 + i * 30, option_text);
        }
    }
    else if (g_game_pid == 0)
    {
        const char* options[]    = {"Soft reboot", "Cold reboot"};
        const int   option_count = sizeof(options) / sizeof(options[0]);

        for (int i = 0; i < option_count; i++)
        {
            char option_text[64];
            if (i == g_selected_option)
            {
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 0;
                g_color_text.rgba.b = 0;

                snprintf(option_text, sizeof(option_text), "> %s", options[i]);
            }
            else
            {
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 255;
                g_color_text.rgba.b = 255;

                snprintf(option_text, sizeof(option_text), "  %s", options[i]);
            }

            gui_print(60, 60 + i * 30, option_text);
        }
    }
    else
    {
        // We're not in a game
        gui_print(60, 60, "Waiting for a game to start...");
    }

    // Restore original color in case it's used elsewhere.
    g_color_text = original_color;
}

// If needed, menu_init can be used to initialize menu-specific resources.
void menu_init(void)
{
    // Currently, nothing to initialize.
}