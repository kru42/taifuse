#include <vitasdkkern.h>
#include <taihen.h>
#include <stdbool.h>
#include <stdint.h>
#include "gui_font_ter-u18b.h" // Your actual font file

#define GUI_WIDTH  960
#define GUI_HEIGHT 544

// Terminus U18 Bold parameters (as used in your original code for 720x408)
// Adjust these if you change resolution.
#define FONT_WIDTH  9
#define FONT_HEIGHT 18
// Each row uses this many bytes (for FONT_WIDTH = 9, that's 2 bytes per row)
#define FONT_BYTES_PER_ROW (((FONT_WIDTH - 1) / 8) + 1)

typedef struct
{
    uint8_t r, g, b, a;
} rgba_t;

// Global GUI buffer and its memblock ID.
static rgba_t* g_gui_buffer;
static SceUID  g_gui_buffer_uid = -1;

// Display frame buffer (assumed to be set up by caller)
static SceDisplayFrameBuf g_gui_fb = {
    .base        = NULL, // Should be set by display init code
    .width       = GUI_WIDTH,
    .height      = GUI_HEIGHT,
    .pitch       = GUI_WIDTH,
    .pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
};

// Menu visibility flag.
bool g_menu_visible = false;

// Allocate a kernel buffer for GUI drawing.
int gui_init(void)
{
    int size         = (GUI_WIDTH * GUI_HEIGHT * sizeof(rgba_t) + 0xfff) & ~0xfff;
    g_gui_buffer_uid = ksceKernelAllocMemBlock("gui", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, size, NULL);
    if (g_gui_buffer_uid < 0)
        return g_gui_buffer_uid;
    ksceKernelGetMemBlockBase(g_gui_buffer_uid, (void**)&g_gui_buffer);
    return 0;
}

void gui_deinit(void)
{
    if (g_gui_buffer_uid >= 0)
        ksceKernelFreeMemBlock(g_gui_buffer_uid);
}

// Clear the GUI buffer (fill with black).
void gui_clear(void)
{
    for (int i = 0; i < GUI_WIDTH * GUI_HEIGHT; i++)
        g_gui_buffer[i] = (rgba_t){0, 0, 0, 255};
}

// Draw a simple semi-transparent menu background.
void gui_draw_menu_background(void)
{
    int x0 = 50, y0 = 50, x1 = 400, y1 = 150;
    for (int y = y0; y < y1; y++)
    {
        for (int x = x0; x < x1; x++)
        {
            g_gui_buffer[y * GUI_WIDTH + x] = (rgba_t){50, 50, 50, 200};
        }
    }
}

// Draw a single character using FONT_TER_U18B.
// The font data is assumed to be arranged as a bitmap with FONT_HEIGHT rows per character,
// each row occupying FONT_BYTES_PER_ROW bytes. Characters are arranged sequentially,
// starting from ASCII 0 (so you might need to adjust if your font starts at space ' ').
void gui_draw_char(char c, int x, int y, rgba_t color)
{
    // Adjust for supported ASCII range if needed.
    // Here we assume the font data begins at ASCII 0.
    int char_index     = (int)c;
    int bytes_per_char = FONT_HEIGHT * FONT_BYTES_PER_ROW;
    int char_offset    = char_index * bytes_per_char;

    for (int row = 0; row < FONT_HEIGHT; row++)
    {
        int row_offset = char_offset + row * FONT_BYTES_PER_ROW;
        for (int col = 0; col < FONT_WIDTH; col++)
        {
            int byte_index = row_offset + (col / 8);
            // Bit order: MSB first.
            if (FONT_TER_U18B[byte_index] & (1 << (7 - (col % 8))))
            {
                int px = x + col;
                int py = y + row;
                if (px < 0 || py < 0 || px >= GUI_WIDTH || py >= GUI_HEIGHT)
                    continue;
                g_gui_buffer[py * GUI_WIDTH + px] = color;
            }
        }
    }
}

// Print a string at (x,y) using the font.
void gui_print(const char* str, int x, int y, rgba_t color)
{
    while (*str)
    {
        gui_draw_char(*str, x, y, color);
        x += FONT_WIDTH; // Advance by font width.
        str++;
    }
}

// Copy the GUI buffer to the display frame buffer.
void gui_copy(void)
{
    for (int line = 0; line < GUI_HEIGHT; line++)
    {
        int dst_offset = line * g_gui_fb.pitch;
        int src_offset = line * GUI_WIDTH;
        ksceKernelMemcpyKernelToUser((uintptr_t)&((rgba_t*)g_gui_fb.base)[dst_offset], &g_gui_buffer[src_offset],
                                     sizeof(rgba_t) * GUI_WIDTH);
    }
}

// Check input and toggle menu when SCE_CTRL_LTRIGGER and SCE_CTRL_RIGHT are pressed.
void gui_input_check(uint32_t buttons)
{
    if ((buttons & SCE_CTRL_LTRIGGER) && (buttons & SCE_CTRL_RIGHT))
        g_menu_visible = !g_menu_visible;
}
