#include "console.h"
#include "gui.h" // For gui_print and gui_printf
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Internal state for the console.
static bool g_console_active = false;
static char g_console_lines[CONSOLE_MAX_LINES][CONSOLE_MAX_LINE_LENGTH];
static int  g_console_line_count = 0;

extern rgba_t g_color_text;

// Starting position on screen for the console.
#define CONSOLE_START_X 15
#define CONSOLE_START_Y 15
// Vertical spacing between console lines.
#define CONSOLE_LINE_SPACING 26

bool g_last_console_active = false;

void console_init(void)
{
    console_clear();
}

void console_deinit(void)
{
    // Nothing to deinitialize in this basic example.
}

void console_toggle(void)
{
    g_console_active = !g_console_active;
}

bool console_is_active(void)
{
    return g_console_active;
}

void console_clear(void)
{
    memset(g_console_lines, 0, sizeof(g_console_lines));
    g_console_line_count = 0;
}

void console_handle_input(SceCtrlButtons buttons)
{
    static bool combo_pressed = false;
    bool toggle_pressed = (buttons & SCE_CTRL_LTRIGGER) && (buttons & SCE_CTRL_RTRIGGER) && (buttons & SCE_CTRL_LEFT);

    if (toggle_pressed && !combo_pressed)
    {
        console_toggle();
        combo_pressed = true;
    }
    else if (!toggle_pressed && combo_pressed)
    {
        combo_pressed = false;
    }
}

void console_log(const char* buffer)
{
    // If we've reached our max, shift all lines up (discard the oldest).
    if (g_console_line_count >= CONSOLE_MAX_LINES)
    {
        for (int i = 0; i < CONSOLE_MAX_LINES - 1; i++)
        {
            memcpy(g_console_lines[i], g_console_lines[i + 1], CONSOLE_MAX_LINE_LENGTH);
        }
        // Only decrement the count by 1 to make room for the new line.
        g_console_line_count = CONSOLE_MAX_LINES - 1;
    }

    // Add the new line at the current position.
    strncpy(g_console_lines[g_console_line_count], buffer, CONSOLE_MAX_LINE_LENGTH - 1);
    g_console_lines[g_console_line_count][CONSOLE_MAX_LINE_LENGTH - 1] = '\0';
    g_console_line_count++;
}

// Draws the static parts of the console (template).
void console_draw_template(void)
{
    if (!console_is_active())
        return;

    // Save the original text color.
    rgba_t original_color = g_color_text;

    // Set a distinct color for the console header.
    g_color_text.rgba.r = 255;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 255;
    gui_print(CONSOLE_START_X, CONSOLE_START_Y, "Console:");

    // Restore original color.
    g_color_text = original_color;
}

// Draws the dynamic parts of the console (the logged lines).
void console_draw_dynamic(void)
{
    if (!console_is_active())
        return;

    // Save the original text color.
    rgba_t original_color = g_color_text;
    // Use a light green for console text.
    g_color_text.rgba.r = 180;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 180;

    int start_y = CONSOLE_START_Y + CONSOLE_LINE_SPACING;
    for (int i = 0; i < g_console_line_count; i++)
    {
        int y = start_y + (i * CONSOLE_LINE_SPACING);
        gui_print(CONSOLE_START_X, y, g_console_lines[i]);
    }

    // Restore the original text color.
    g_color_text = original_color;
}
