#include "hex_browser.h"
#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "gui.h"
#include "log.h"
#include "menu.h"

// External variables from other files
extern int                  g_game_pid;
extern rgba_t               g_color_text;
extern rgba_t               g_color_bg;
extern const unsigned char* g_gui_font;
extern unsigned char        g_gui_font_width;
extern unsigned char        g_gui_font_height;
extern bool                 ui_needs_redraw;

// Function to convert ASCII hex char to value
static inline int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

// States and constants for the hex browser
#define HEX_ROWS_PER_PAGE  16
#define HEX_BYTES_PER_ROW  16
#define HEX_ADDR_INPUT_MAX 16 // Maximum hex digits for address input

typedef enum
{
    HEX_MODE_NAVIGATION,
    HEX_MODE_EDIT_ADDR,
    HEX_MODE_EDIT_VALUE
} hex_mode_t;

static bool       hex_browser_active = false;
static hex_mode_t current_mode       = HEX_MODE_NAVIGATION;

// Memory view state
static uintptr_t current_address = 0x00000000;
static uintptr_t cursor_address  = 0x00000000;
static int       cursor_x        = 0; // Position within row (0-15)
static int       cursor_y        = 0; // Row within current page (0-15)

// Address input state
static char address_input[HEX_ADDR_INPUT_MAX + 1] = {0};
static int  address_input_pos                     = 0;

// Value edit state
static char value_input[3]  = {0}; // 2 hex digits + null
static int  value_input_pos = 0;

// Buffer for reading memory from the game process
static uint8_t memory_buffer[HEX_ROWS_PER_PAGE * HEX_BYTES_PER_ROW];

// Prototypes for internal functions
static int  read_game_memory(void);
static void draw_hex_view(void);
static void draw_status_bar(void);
static void handle_navigation_input(SceCtrlButtons buttons);
static void handle_address_input(SceCtrlButtons buttons, unsigned int button_pressed);
static void handle_value_input(SceCtrlButtons buttons, unsigned int button_pressed);
static int  write_game_memory(uintptr_t addr, uint8_t value);
static void commit_value_edit(void);

//--------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------

// Initialize the hex browser
void hex_browser_init(void)
{
    hex_browser_active = false;
    current_mode       = HEX_MODE_NAVIGATION;
    current_address    = 0x00000000;
    cursor_address     = 0x00000000;
    cursor_x           = 0;
    cursor_y           = 0;

    memset(address_input, 0, sizeof(address_input));
    address_input_pos = 0;

    memset(value_input, 0, sizeof(value_input));
    value_input_pos = 0;
}

// Toggle the hex browser active state
void hex_browser_toggle(void)
{
    if (!g_game_pid)
    {
        // No game is running
        return;
    }

    hex_browser_active = !hex_browser_active;
    if (hex_browser_active)
    {
        // When activating, ensure we're in navigation mode
        current_mode = HEX_MODE_NAVIGATION;
        // Read initial memory page
        read_game_memory();
    }
}

// Check if hex browser is active
bool hex_browser_is_active(void)
{
    return hex_browser_active;
}

void hex_browser_handle_input(SceCtrlButtons buttons)
{
    static bool combo_pressed = false;
    bool toggle_pressed = (buttons & SCE_CTRL_LTRIGGER) && (buttons & SCE_CTRL_RTRIGGER) && (buttons & SCE_CTRL_DOWN);

    if (toggle_pressed && !combo_pressed)
    {
        hex_browser_toggle();
        combo_pressed   = true;
    }
    else if (!toggle_pressed && combo_pressed)
    {
        combo_pressed = false;
    }

    unsigned int button_pressed = buttons;

    if (button_pressed & SCE_CTRL_SELECT)
    {
        if (current_mode == HEX_MODE_NAVIGATION)
        {
            current_mode = HEX_MODE_EDIT_ADDR;
            snprintf(address_input, sizeof(address_input), "%08lX", current_address);
            address_input_pos = 0;
        }
        else
        {
            current_mode = HEX_MODE_NAVIGATION;
        }
        return;
    }

    switch (current_mode)
    {
    case HEX_MODE_NAVIGATION:
        handle_navigation_input(buttons);
        break;
    case HEX_MODE_EDIT_ADDR:
        handle_address_input(buttons, button_pressed);
        break;
    case HEX_MODE_EDIT_VALUE:
        handle_value_input(buttons, button_pressed);
        break;
    }

    ui_needs_redraw = true; // Any interaction with the hex browser should trigger a redraw
}

// Draw the hex browser UI
void hex_browser_draw(void)
{
    if (!hex_browser_active)
        return;

    // Update memory content if needed
    read_game_memory();

    // Draw the hex display
    draw_hex_view();

    // Draw status bar with controls
    draw_status_bar();
}

//--------------------------------------------------------------------
// Internal implementation
//--------------------------------------------------------------------

// Read memory from the game process at the current address
static int read_game_memory(void)
{
    if (!g_game_pid)
        return -1;

    // Align address to 16-byte boundary for clean display
    uintptr_t aligned_addr = current_address & ~0xF;
    int       size         = HEX_ROWS_PER_PAGE * HEX_BYTES_PER_ROW;

    // Use kernel function to read from user memory
    int ret = ksceKernelMemcpyUserToKernel(memory_buffer, (uintptr_t)aligned_addr, size);
    if (ret < 0)
    {
        LOG("Failed to read memory at 0x%08lX, error: 0x%08X", aligned_addr, ret);
        memset(memory_buffer, 0xCC, size); // Fill with pattern to indicate error
        return ret;
    }

    return 0;
}

// Draw the hex view of memory
static void draw_hex_view(void)
{
    rgba_t    original_color = g_color_text;
    uintptr_t aligned_addr   = current_address & ~0xF;

    // Title
    g_color_text.rgba.r = 255;
    g_color_text.rgba.g = 255;
    g_color_text.rgba.b = 255;
    gui_print(50, 30, "Memory Browser - PID:");
    gui_printf(230, 30, "%08X", g_game_pid);

    // Mode indicator
    const char* mode_str = "NAVIGATION";
    if (current_mode == HEX_MODE_EDIT_ADDR)
        mode_str = "EDIT ADDRESS";
    else if (current_mode == HEX_MODE_EDIT_VALUE)
        mode_str = "EDIT VALUE";
    gui_print(400, 30, mode_str);

    // Address input display during address edit mode
    if (current_mode == HEX_MODE_EDIT_ADDR)
    {
        gui_print(50, 60, "Go to:");
        gui_print(120, 60, address_input);
        // Draw cursor for editing
        if ((ksceKernelGetSystemTimeWide() / 500000) % 2 == 0)
        {
            gui_print(120 + address_input_pos * g_gui_font_width, 60, "_");
        }
    }
    else
    {
        // Show current address in navigation mode
        gui_print(50, 60, "Address:");
        gui_printf(120, 60, "0x%08lX", aligned_addr);
    }

    // Memory hex display with 16 bytes per row
    int y_pos = 90;

    // Column headers
    gui_print(50, y_pos, "Address    |  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  | ASCII");
    y_pos += 20;
    gui_print(50, y_pos, "-----------|--------------------------------------------------|--------");
    y_pos += 20;

    for (int row = 0; row < HEX_ROWS_PER_PAGE; row++)
    {
        // Print address for this row
        gui_printf(50, y_pos, "%08lX |", aligned_addr + row * HEX_BYTES_PER_ROW);

        // Hex values (16 per row)
        for (int col = 0; col < HEX_BYTES_PER_ROW; col++)
        {
            int       idx       = row * HEX_BYTES_PER_ROW + col;
            uintptr_t byte_addr = aligned_addr + idx;

            // Highlight cursor position if in navigation or value edit mode
            if ((current_mode == HEX_MODE_NAVIGATION || current_mode == HEX_MODE_EDIT_VALUE) && row == cursor_y &&
                col == cursor_x)
            {
                // Highlight the current cursor position
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 128;
                g_color_text.rgba.b = 0;

                if (current_mode == HEX_MODE_EDIT_VALUE)
                {
                    // Show input value instead of memory value
                    gui_printf(145 + col * 24, y_pos, "%s", value_input);
                    // Add cursor for value editing
                    if ((ksceKernelGetSystemTimeWide() / 500000) % 2 == 0 && value_input_pos < 2)
                    {
                        gui_print(145 + col * 24 + value_input_pos * (g_gui_font_width / 2), y_pos, "_");
                    }
                }
                else
                {
                    gui_printf(145 + col * 24, y_pos, "%02X", memory_buffer[idx]);
                }
            }
            else
            {
                // Normal display of memory values
                g_color_text.rgba.r = 255;
                g_color_text.rgba.g = 255;
                g_color_text.rgba.b = 255;
                gui_printf(145 + col * 24, y_pos, "%02X", memory_buffer[idx]);
            }
        }

        // ASCII representation
        gui_print(530, y_pos, "|");

        g_color_text.rgba.r = 200;
        g_color_text.rgba.g = 200;
        g_color_text.rgba.b = 200;

        for (int col = 0; col < HEX_BYTES_PER_ROW; col++)
        {
            int  idx = row * HEX_BYTES_PER_ROW + col;
            char c   = memory_buffer[idx];
            // Only display printable ASCII characters, otherwise show a dot
            if (c >= 32 && c <= 126)
            {
                char str[2] = {c, '\0'};
                gui_print(540 + col * g_gui_font_width, y_pos, str);
            }
            else
            {
                gui_print(540 + col * g_gui_font_width, y_pos, ".");
            }
        }

        y_pos += 20;
    }

    // Restore original text color
    g_color_text = original_color;
}

// Draw the status bar with control hints
static void draw_status_bar(void)
{
    rgba_t original_color = g_color_text;
    g_color_text.rgba.r   = 150;
    g_color_text.rgba.g   = 150;
    g_color_text.rgba.b   = 150;

    int y_pos = 460;

    // Different controls based on mode
    if (current_mode == HEX_MODE_NAVIGATION)
    {
        gui_print(50, y_pos, "D-Pad: Navigate | L/R: Page | Triangle: Exit | Select: Address | Circle: Edit Value");
    }
    else if (current_mode == HEX_MODE_EDIT_ADDR)
    {
        gui_print(50, y_pos, "D-Pad: Input | Cross: Confirm | Select/Triangle: Cancel");
    }
    else if (current_mode == HEX_MODE_EDIT_VALUE)
    {
        gui_print(50, y_pos, "D-Pad: Input | Cross: Confirm | Select/Triangle: Cancel");
    }

    // Restore original text color
    g_color_text = original_color;
}

// Update the cursor_address based on current cursor position and page
static void update_cursor_address(void)
{
    uintptr_t aligned_addr = current_address & ~0xF;
    cursor_address         = aligned_addr + (cursor_y * HEX_BYTES_PER_ROW) + cursor_x;
}

// Write a byte to the game's memory
static int write_game_memory(uintptr_t addr, uint8_t value)
{
    if (!g_game_pid)
        return -1;

    uint8_t byte_value = value;
    int     ret        = ksceKernelMemcpyKernelToUser((uintptr_t)addr, &byte_value, 1);
    if (ret < 0)
    {
        LOG("Failed to write memory at 0x%08lX, error: 0x%08X", addr, ret);
        return ret;
    }

    return 0;
}

// Handle input in navigation mode
static void handle_navigation_input(SceCtrlButtons buttons)
{
    // Navigation with D-pad
    if (buttons & SCE_CTRL_UP)
    {
        if (cursor_y > 0)
        {
            cursor_y--;
        }
        else
        {
            // Move to previous page
            current_address -= HEX_BYTES_PER_ROW;
            update_cursor_address();
        }
    }

    if (buttons & SCE_CTRL_DOWN)
    {
        if (cursor_y < HEX_ROWS_PER_PAGE - 1)
        {
            cursor_y++;
        }
        else
        {
            // Move to next page
            current_address += HEX_BYTES_PER_ROW;
            update_cursor_address();
        }
    }

    if (buttons & SCE_CTRL_LEFT)
    {
        if (cursor_x > 0)
        {
            cursor_x--;
        }
        else if (cursor_y > 0)
        {
            // Wrap to end of previous line
            cursor_x = HEX_BYTES_PER_ROW - 1;
            cursor_y--;
        }
        else
        {
            // Move to previous page
            current_address -= HEX_BYTES_PER_ROW;
            cursor_x = HEX_BYTES_PER_ROW - 1;
            update_cursor_address();
        }
    }

    if (buttons & SCE_CTRL_RIGHT)
    {
        if (cursor_x < HEX_BYTES_PER_ROW - 1)
        {
            cursor_x++;
        }
        else if (cursor_y < HEX_ROWS_PER_PAGE - 1)
        {
            // Wrap to start of next line
            cursor_x = 0;
            cursor_y++;
        }
        else
        {
            // Move to next page
            current_address += HEX_BYTES_PER_ROW;
            cursor_x = 0;
            update_cursor_address();
        }
    }

    // Page navigation with L/R triggers
    if (buttons & SCE_CTRL_LTRIGGER)
    {
        // Page up (one full screen)
        current_address -= HEX_BYTES_PER_ROW * HEX_ROWS_PER_PAGE;
        update_cursor_address();
    }

    if (buttons & SCE_CTRL_RTRIGGER)
    {
        // Page down (one full screen)
        current_address += HEX_BYTES_PER_ROW * HEX_ROWS_PER_PAGE;
        update_cursor_address();
    }

    // Switch to value edit mode
    if (buttons & SCE_CTRL_CIRCLE)
    {
        current_mode = HEX_MODE_EDIT_VALUE;
        // Initialize with current value
        snprintf(value_input, sizeof(value_input), "%02X", memory_buffer[cursor_y * HEX_BYTES_PER_ROW + cursor_x]);
        value_input_pos = 0;
    }

    // Exit hex browser
    if (buttons & SCE_CTRL_TRIANGLE)
    {
        hex_browser_active = false;
    }
}

static void handle_address_input(SceCtrlButtons buttons, unsigned int button_pressed)
{
    if (button_pressed & SCE_CTRL_CROSS)
    {
        // Confirm address input and switch to navigation mode
        current_address = strtoul(address_input, NULL, 16);
        current_mode    = HEX_MODE_NAVIGATION;
        read_game_memory();
        return;
    }

    if (button_pressed & SCE_CTRL_SELECT || button_pressed & SCE_CTRL_TRIANGLE)
    {
        // Cancel address input and return to navigation mode
        current_mode = HEX_MODE_NAVIGATION;
        return;
    }

    // Handle address input with D-pad and buttons
    if (buttons & SCE_CTRL_UP || buttons & SCE_CTRL_DOWN || buttons & SCE_CTRL_LEFT || buttons & SCE_CTRL_RIGHT)
    {
        // Move cursor position in address input
        if (buttons & SCE_CTRL_LEFT && address_input_pos > 0)
        {
            address_input_pos--;
        }
        else if (buttons & SCE_CTRL_RIGHT && address_input_pos < HEX_ADDR_INPUT_MAX - 1)
        {
            address_input_pos++;
        }
    }
    else
    {
        // Handle character input in address field (hex digits)
        char input_char = 0;
        if (buttons & SCE_CTRL_CIRCLE)
            input_char = '0';
        if (buttons & SCE_CTRL_SQUARE)
            input_char = '1';
        if (buttons & SCE_CTRL_TRIANGLE)
            input_char = '2'; // Extend with other buttons for hex characters

        if (input_char && address_input_pos < HEX_ADDR_INPUT_MAX - 1)
        {
            address_input[address_input_pos++] = input_char;
            address_input[address_input_pos]   = '\0'; // Null-terminate the string
        }
    }
}

static void handle_value_input(SceCtrlButtons buttons, unsigned int button_pressed)
{
    if (button_pressed & SCE_CTRL_CROSS)
    {
        // Confirm the value edit and write to memory
        write_game_memory(cursor_address, (uint8_t)strtol(value_input, NULL, 16));
        current_mode = HEX_MODE_NAVIGATION;
        return;
    }

    if (button_pressed & SCE_CTRL_SELECT || button_pressed & SCE_CTRL_TRIANGLE)
    {
        // Cancel value edit and return to navigation mode
        current_mode = HEX_MODE_NAVIGATION;
        return;
    }

    // Handle value input with D-pad and buttons
    if (buttons & SCE_CTRL_LEFT || buttons & SCE_CTRL_RIGHT)
    {
        if (buttons & SCE_CTRL_LEFT && value_input_pos > 0)
        {
            value_input_pos--;
        }
        else if (buttons & SCE_CTRL_RIGHT && value_input_pos < 1)
        {
            value_input_pos++;
        }
    }
    else
    {
        // Handle character input for value (hex digits)
        char input_char = 0;
        if (buttons & SCE_CTRL_CIRCLE)
            input_char = '0';
        if (buttons & SCE_CTRL_SQUARE)
            input_char = '1';
        if (buttons & SCE_CTRL_TRIANGLE)
            input_char = '2'; // Extend with other buttons for hex characters

        if (input_char && value_input_pos < 2)
        {
            value_input[value_input_pos++] = input_char;
            value_input[value_input_pos]   = '\0'; // Null-terminate the string
        }
    }
}

static void commit_value_edit(void)
{
    // Write the updated value to the memory
    uint8_t value = (uint8_t)strtol(value_input, NULL, 16);
    int     ret   = write_game_memory(cursor_address, value);
    if (ret == 0)
    {
        // Successfully written
        LOG("Value written at 0x%08lX: 0x%02X", cursor_address, value);
    }
    else
    {
        LOG("Failed to write value at 0x%08lX", cursor_address);
    }
}
