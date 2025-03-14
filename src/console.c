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
#define CONSOLE_START_X 5
#define CONSOLE_START_Y 15
// Vertical spacing between console lines.
#define CONSOLE_LINE_SPACING 26

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

// Process input to toggle the console.
// The toggle combo is: LTRIGGER + RTRIGGER + LEFT.
void console_handle_input(SceCtrlButtons buttons)
{
    static bool combo_pressed = false;
    // Check if the toggle combo is pressed.
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

void console_log(const char* format, ...)
{
    char    buffer[CONSOLE_MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // If we've reached our max, shift all lines up (discard the oldest).
    if (g_console_line_count >= CONSOLE_MAX_LINES)
    {
        for (int i = 1; i < CONSOLE_MAX_LINES; i++)
        {
            memcpy(g_console_lines[i - 1], g_console_lines[i], CONSOLE_MAX_LINE_LENGTH);
        }
        g_console_line_count = CONSOLE_MAX_LINES - 1;
    }
    // Add the new line.
    strncpy(g_console_lines[g_console_line_count], buffer, CONSOLE_MAX_LINE_LENGTH - 1);
    g_console_lines[g_console_line_count][CONSOLE_MAX_LINE_LENGTH - 1] = '\0';
    g_console_line_count++;
}

void console_draw(void)
{
    if (!console_is_active())
        return;

    gui_clear();

    // Optionally, set a distinct color for the console text.
    // Save the current text color.
    rgba_t original_color = g_color_text;
    // Use a light green for console text.
    g_color_text.rgba.r = 180;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 180;

    // Draw a header for clarity.
    gui_print(CONSOLE_START_X, CONSOLE_START_Y, "Console:");

    // Draw each logged line.
    for (int i = 0; i < g_console_line_count; i++)
    {
        int y = CONSOLE_START_Y + i * CONSOLE_LINE_SPACING;
        gui_print(CONSOLE_START_X, y, g_console_lines[i]);
    }

    // Restore the original text color.
    g_color_text = original_color;
}
