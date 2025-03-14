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

// Process input for menu navigation and actions.
// This function both handles the toggle combo (UP + LTRIGGER + RTRIGGER)
// and menu-specific button presses.
void menu_handle_input(SceCtrlButtons buttons)
{
    static bool combo_pressed = false;
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

    // If the menu is active and we're in a game, show the cheat menu
    if (g_menu_active)
    {
        // Simple navigation: adjust selected option based on input.
        // (Assuming SCE_CTRL_UP/DOWN represent new presses for simplicity.
        //  In a more robust design, you might want to debounce or track previous state.)
        if (buttons & SCE_CTRL_UP)
        {
            g_selected_option--;
        }
        if (buttons & SCE_CTRL_DOWN)
        {
            g_selected_option++;
        }

        const int option_count = 3;
        if (g_selected_option < 0)
            g_selected_option = option_count - 1;
        else if (g_selected_option >= option_count)
            g_selected_option = 0;

        // Process confirmation
        if (buttons & SCE_CTRL_CIRCLE) // TODO: Use the correct confirm button
        {
            if (g_selected_option == 0)
            {
                // Reload cheats.
                free_cheats(g_cheat_groups, g_cheat_group_count);
                if (load_cheats("ux0:data/taifuse/cheats.ini", &g_cheat_groups, &g_cheat_group_count) == 0)
                {
                    set_cheats_enabled_for_game(g_cheat_groups, g_cheat_group_count, g_titleid, true);
                }
            }
            else if (g_selected_option == 1)
            {
                // Terminate the game process
                if (sceKernelKillProcessForKernel == NULL)
                {
                    if (module_get_export_func(KERNEL_PID, "SceProcessmgr", TAI_ANY_LIBRARY, 0xF388F05C,
                                               &sceKernelKillProcessForKernel) < 1)
                    {
                        LOG("Failed to find SceProcessmgr::sceKernelKillProcessForKernel, operation aborted");
                        return;
                    }
                }

                sceKernelKillProcessForKernel(g_game_pid, 0x2);
            }
            else if (g_selected_option == 2)
            {
                // Exit the menu.
                g_menu_active = false;
            }
        }
    }
}

void menu_draw(void)
{
    if (!menu_is_active())
        return;

    gui_clear();

    // Save the original text color.
    rgba_t original_color = g_color_text;

    // Ensure title is drawn in white.
    g_color_text.rgba.r = 255;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 255;
    gui_print(50, 30, "Taifuse Menu");

    if (g_game_pid != 0)
    {
        char reload_option[64];
        snprintf(reload_option, sizeof(reload_option), "Reload cheats for %s", g_titleid);

        const char* options[]    = {reload_option, "Kill game process", "Exit"};
        const int   option_count = sizeof(options) / sizeof(options[0]);

        for (int i = 0; i < option_count; i++)
        {
            if (i == g_selected_option)
            {
                // Red highlight for selected option.
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 0;
                g_color_text.rgba.b = 0;

                // Add cursor indicator for selected option
                gui_print(45, 60 + i * 30, ">");
            }
            else
            {
                // White for non-selected options.
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 255;
                g_color_text.rgba.b = 255;
            }

            gui_print(60, 60 + i * 30, options[i]);
        }
    }
    else
    {
        // We're not in a game
        gui_print(60, 15, "Waiting for a game to start...");
    }

    // Restore original color in case it's used elsewhere.
    g_color_text = original_color;
}

// If needed, menu_init can be used to initialize menu-specific resources.
void menu_init(void)
{
    // Currently, nothing to initialize.
}
