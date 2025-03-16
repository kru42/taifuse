#include "hex_browser.h"
#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "gui.h"
#include "log.h"
#include "menu.h"

// External variables and unchanged globals...
extern int                  g_game_pid;
extern rgba_t               g_color_text;
extern rgba_t               g_color_bg;
extern const unsigned char* g_gui_font;
extern unsigned char        g_gui_font_width;
extern unsigned char        g_gui_font_height;

//------------------------------------------------------------
// Layout and display constants
//------------------------------------------------------------
#define HEX_BROWSER_X_START     24
#define HEX_BROWSER_Y_START     24
#define HEX_TITLE_Y_OFFSET      0
#define HEX_MODE_X_OFFSET       376 // X position for mode indicator
#define HEX_ADDR_Y_OFFSET       30  // Y offset for address display
#define HEX_HEADER_Y_OFFSET     50  // Y offset for column headers
#define HEX_DATA_Y_OFFSET       100 // Y offset for memory data display
#define HEX_GOTO_X_OFFSET       75  // X offset for goto address input
#define HEX_COLUMN_HEADER_LINE  70  // Y offset for column header line
#define HEX_STATUS_BAR_Y_OFFSET 430 // Y offset for status bar
#define HEX_STATUS_BAR_X_OFFSET 50  // X offset for status bar text
#define HEX_STATUS_BAR_Y        460 // Absolute Y position for status bar

// Layout for hex values display
#define HEX_ADDR_COLUMN_WIDTH 120 // Width of address column
#define HEX_BYTE_WIDTH        28  // Width for each byte display (including spacing)
#define HEX_ASCII_X_OFFSET    530 // X position for ASCII section
#define HEX_ASCII_SEPARATOR_X 530 // X position for separator before ASCII
#define HEX_ASCII_START_X     540 // X position for ASCII characters

// Row and content configuration
#define HEX_ROW_HEIGHT     20 // Height of each memory row
#define HEX_ROWS_PER_PAGE  16 // Number of rows displayed per page
#define HEX_BYTES_PER_ROW  16 // Number of bytes per row
#define HEX_ADDR_INPUT_MAX 16 // Maximum input length for address

// Colors
#define COLOR_WHITE     {.rgba = {255, 255, 255, 255}} // White color for titles and text
#define COLOR_ORANGE    {.rgba = {255, 128, 0, 255}}   // Orange highlight for cursor
#define COLOR_LIGHTGRAY {.rgba = {200, 200, 200, 255}} // Light gray for ASCII
#define COLOR_STATUSBAR {.rgba = {150, 150, 150, 255}} // Light gray for status bar

//------------------------------------------------------------
// Unchanged helpers & definitions
//------------------------------------------------------------
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

#define HEX_ROWS_PER_PAGE  16
#define HEX_BYTES_PER_ROW  16
#define HEX_ADDR_INPUT_MAX 16

typedef enum
{
    HEX_MODE_NAVIGATION,
    HEX_MODE_EDIT_ADDR,
    HEX_MODE_EDIT_VALUE
} hex_mode_t;

// Hex browser state
static bool       hex_browser_active = false;
static hex_mode_t current_mode       = HEX_MODE_NAVIGATION;

static uintptr_t current_address = 0x00000000;
static uintptr_t cursor_address  = 0x00000000;
static int       cursor_x        = 0;
static int       cursor_y        = 0;

// Address input state
static char address_input[HEX_ADDR_INPUT_MAX + 1] = {0};
static int  address_input_pos                     = 0;

// Value edit state
static char value_input[3]  = {0}; // 2 hex digits + null
static int  value_input_pos = 0;

// Buffer for memory reading
static uint8_t memory_buffer[HEX_ROWS_PER_PAGE * HEX_BYTES_PER_ROW];

bool        g_last_hexbrowser_active = false;
static bool memory_needs_update      = true;

//------------------------------------------------------------
// Prototypes for internal functions
//------------------------------------------------------------
static int  read_game_memory(void);
static void draw_status_bar(void);
static void handle_navigation_input(SceCtrlButtons buttons);
static void handle_address_input(SceCtrlButtons buttons, unsigned int button_pressed);
static void handle_value_input(SceCtrlButtons buttons, unsigned int button_pressed);
static int  write_game_memory(uintptr_t addr, uint8_t value);
static void update_cursor_address(void);
static void commit_value_edit(void);

//------------------------------------------------------------
// Public API (unchanged except for drawing refactor)
//------------------------------------------------------------
void hex_browser_init(void)
{
    hex_browser_active = false;
    current_mode       = HEX_MODE_NAVIGATION;
    current_address    = 0x81000000; // eboot.bin base address
    cursor_address     = 0x81000000;
    cursor_x           = 0;
    cursor_y           = 0;
    memset(address_input, 0, sizeof(address_input));
    address_input_pos = 0;
    memset(value_input, 0, sizeof(value_input));
    value_input_pos = 0;
}

void hex_browser_toggle(void)
{
    if (!g_game_pid)
        return;
    hex_browser_active = !hex_browser_active;
    if (hex_browser_active)
    {
        current_mode = HEX_MODE_NAVIGATION;
    }
}

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
        combo_pressed = true;
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
}

//------------------------------------------------------------
// Internal implementations
//------------------------------------------------------------
static int read_game_memory(void)
{
    if (!g_game_pid)
        return -1;
    uintptr_t aligned_addr = current_address & ~0xF;
    int       size         = HEX_ROWS_PER_PAGE * HEX_BYTES_PER_ROW;
    int       ret          = ksceKernelMemcpyUserToKernel(memory_buffer, (uintptr_t)aligned_addr, size);
    if (ret < 0)
    {
        LOG("Failed to read memory at 0x%08lX, error: 0x%08X", aligned_addr, ret);
        memset(memory_buffer, 0xCC, size); // Error pattern
        return ret;
    }
    return 0;
}

void hex_browser_draw_template(void)
{
    char text_buf[256];

    rgba_t original_color = g_color_text;
    rgba_t title_color    = COLOR_WHITE; // Use the defined color constant

    // Save original color and set to title color
    g_color_text = title_color;

    // Title and PID - make sure these are visible with proper spacing
    snprintf(text_buf, sizeof(text_buf), "Memory Browser - PID: %08x", g_game_pid);
    gui_print(HEX_BROWSER_X_START, HEX_BROWSER_Y_START, text_buf);

    // Mode indicator with clear spacing
    const char* mode_str = "NAVIGATION";
    if (current_mode == HEX_MODE_EDIT_ADDR)
        mode_str = "EDIT ADDRESS";
    else if (current_mode == HEX_MODE_EDIT_VALUE)
        mode_str = "EDIT VALUE";
    gui_print(HEX_MODE_X_OFFSET, HEX_BROWSER_Y_START, mode_str);

    // Address display (when not editing address)
    if (current_mode != HEX_MODE_EDIT_ADDR)
    {
        gui_printf(HEX_BROWSER_X_START, HEX_BROWSER_Y_START + HEX_ADDR_Y_OFFSET, "Address: 0x%08lX",
                   current_address & ~0xF);
    }

    // Column headers for hex view with proper spacing
    int y_pos = HEX_BROWSER_Y_START + HEX_HEADER_Y_OFFSET;

    // Address column
    gui_print(HEX_BROWSER_X_START, y_pos, "Address");

    // Separator
    gui_print(HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH - 10, y_pos, "|");

    // Hex bytes header
    int hex_start_x = HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH;
    for (int i = 0; i < HEX_BYTES_PER_ROW; i++)
    {
        gui_printf(hex_start_x + i * HEX_BYTE_WIDTH, y_pos, "%X", i);
    }

    // ASCII separator and header
    gui_print(HEX_ASCII_SEPARATOR_X, y_pos, "| ASCII");

    // Separator line
    y_pos += 20;
    gui_print(HEX_BROWSER_X_START, y_pos, "-----------");
    gui_print(HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH - 10, y_pos, "|");

    for (int i = 0; i < HEX_BYTES_PER_ROW * 3 - 2; i++)
    {
        gui_print(HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH + i * 8, y_pos, "-");
    }

    gui_print(HEX_ASCII_SEPARATOR_X, y_pos, "|");
    for (int i = 0; i < 16; i++)
    {
        gui_print(HEX_ASCII_START_X + i * g_gui_font_width, y_pos, "-");
    }

    draw_status_bar();

    // Restore original text color
    g_color_text = original_color;
}

void hex_browser_draw_dynamic(void)
{
    char text_buf[256];

    rgba_t original_color  = g_color_text;
    rgba_t highlight_color = COLOR_ORANGE;    // Orange highlight for cursor
    rgba_t normal_color    = COLOR_WHITE;     // White for normal text
    rgba_t ascii_color     = COLOR_LIGHTGRAY; // Light gray for ASCII

    uintptr_t aligned_addr = current_address & ~0xF;
    int       y_pos        = HEX_BROWSER_Y_START + HEX_DATA_Y_OFFSET; // Starting Y position for dynamic memory view

    if (memory_needs_update)
    {
        read_game_memory();
        memory_needs_update = false;
    }

    // In address edit mode, show input field with blinking cursor.
    if (current_mode == HEX_MODE_EDIT_ADDR)
    {
        g_color_text = normal_color;
        gui_printf(HEX_BROWSER_X_START, HEX_BROWSER_Y_START + HEX_ADDR_Y_OFFSET, "Go to: %s", address_input);

        if ((ksceKernelGetSystemTimeWide() / 500000) % 2 == 0)
        {
            gui_print(HEX_BROWSER_X_START + HEX_GOTO_X_OFFSET + address_input_pos * g_gui_font_width,
                      HEX_BROWSER_Y_START + HEX_ADDR_Y_OFFSET, "_");
        }
    }

    // Draw memory content (hex values and ASCII)
    for (int row = 0; row < HEX_ROWS_PER_PAGE; row++)
    {
        // Set color for address column
        g_color_text = normal_color;
        // Address column with proper alignment
        gui_printf(HEX_BROWSER_X_START, y_pos, "%08lX", aligned_addr + row * HEX_BYTES_PER_ROW);

        // Separator between address and hex values
        gui_print(HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH - 10, y_pos, "|");

        // Draw hex values
        for (int col = 0; col < HEX_BYTES_PER_ROW; col++)
        {
            int idx = row * HEX_BYTES_PER_ROW + col;

            // Calculate correct position for each hex byte
            int x_pos = HEX_BROWSER_X_START + HEX_ADDR_COLUMN_WIDTH + col * HEX_BYTE_WIDTH;

            // Highlight cursor position in navigation or value edit modes
            if ((current_mode == HEX_MODE_NAVIGATION || current_mode == HEX_MODE_EDIT_VALUE) && row == cursor_y &&
                col == cursor_x)
            {
                g_color_text = highlight_color;

                if (current_mode == HEX_MODE_EDIT_VALUE)
                {
                    gui_printf(x_pos, y_pos, "%s", value_input);
                    if ((ksceKernelGetSystemTimeWide() / 500000) % 2 == 0 && value_input_pos < 2)
                    {
                        gui_print(x_pos + value_input_pos * (g_gui_font_width / 2), y_pos, "_");
                    }
                }
                else
                {
                    gui_printf(x_pos, y_pos, "%02X", memory_buffer[idx]);
                }
            }
            else
            {
                g_color_text = normal_color;
                gui_printf(x_pos, y_pos, "%02X", memory_buffer[idx]);
            }
        }

        // Draw separator before ASCII representation
        g_color_text = normal_color;
        gui_print(HEX_ASCII_SEPARATOR_X, y_pos, "|");

        // Draw ASCII representation for the row
        g_color_text = ascii_color;

        for (int col = 0; col < HEX_BYTES_PER_ROW; col++)
        {
            int  idx = row * HEX_BYTES_PER_ROW + col;
            char c   = memory_buffer[idx];

            // Calculate correct position for each ASCII character
            int x_pos = HEX_ASCII_START_X + col * g_gui_font_width;

            if (c >= 32 && c <= 126)
            {
                char str[2] = {c, '\0'};
                gui_print(x_pos, y_pos, str);
            }
            else
            {
                gui_print(x_pos, y_pos, ".");
            }
        }

        y_pos += HEX_ROW_HEIGHT;
    }

    // Restore original color
    g_color_text = original_color;
}

// Status bar drawing function
static void draw_status_bar(void)
{
    rgba_t original_color = g_color_text;
    rgba_t status_color   = COLOR_STATUSBAR; // Light gray for status bar

    g_color_text = status_color;

    if (current_mode == HEX_MODE_NAVIGATION)
    {
        gui_print(HEX_STATUS_BAR_X_OFFSET, HEX_STATUS_BAR_Y,
                  "D-Pad: Navigate | L/R: Page | Triangle: Exit | Select: Address | Circle: Edit Value");
    }
    else if (current_mode == HEX_MODE_EDIT_ADDR)
    {
        gui_print(HEX_STATUS_BAR_X_OFFSET, HEX_STATUS_BAR_Y, "D-Pad: Input | Cross: Confirm | Select/Triangle: Cancel");
    }
    else if (current_mode == HEX_MODE_EDIT_VALUE)
    {
        gui_print(HEX_STATUS_BAR_X_OFFSET, HEX_STATUS_BAR_Y, "D-Pad: Input | Cross: Confirm | Select/Triangle: Cancel");
    }

    // Restore original color
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

static void handle_navigation_input(SceCtrlButtons buttons)
{
    // Store previous values
    int       prev_cursor_x = cursor_x;
    int       prev_cursor_y = cursor_y;
    uintptr_t prev_address  = current_address;

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

    if (prev_cursor_x != cursor_x || prev_cursor_y != cursor_y || prev_address != current_address)
    {
        memory_needs_update = true;
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
