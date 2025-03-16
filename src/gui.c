#include "gui.h"
#include <vitasdkkern.h>
#include <stdbool.h>

#include "log.h"
#include "cheats.h"
#include "menu.h"
#include "console.h"
#include "hex_browser.h"
#include "fonts/gui_font_ter-u24b.h"

// Global variables for our internal GUI buffer and display framebuffer.
static SceDisplayFrameBuf g_gui_fb = {
    .width  = 960,
    .height = 544,
    .pitch  = 960,
    .base   = NULL,
};
static float g_gui_fb_w_ratio = 1.0f;
static float g_gui_fb_h_ratio = 1.0f;

static rgba_t* g_gui_buffer;
static SceUID  g_gui_buffer_uid = -1;

// Font parameters (using your supplied 12x24 font)
const unsigned char* g_gui_font        = FONT_TER_U24B;
unsigned char        g_gui_font_width  = GUI_FONT_W;
unsigned char        g_gui_font_height = GUI_FONT_H;
float                g_gui_font_scale  = 1.0f;

// Colors for rendering
rgba_t g_color_text      = {.rgba = {255, 255, 255, 255}};
rgba_t g_color_bg        = {.rgba = {0, 0, 128, 255}};
rgba_t g_color_highlight = {.rgba = {0, 255, 255, 255}};

bool g_ui_state_changed = false;

extern bool g_last_menu_active;
extern bool g_last_console_active;
extern bool g_last_hexbrowser_active;

//--------------------------------------------------------------------
// Initialization / deinitialization
//--------------------------------------------------------------------
int gui_init(void)
{
    // Allocate our internal GUI buffer (aligned to page size)
    int size         = (GUI_WIDTH * GUI_HEIGHT * sizeof(rgba_t) + 0xfff) & ~0xfff;
    g_gui_buffer_uid = ksceKernelAllocMemBlock("gui_buffer", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, size, NULL);
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

//--------------------------------------------------------------------
// Framebuffer and scaling
//--------------------------------------------------------------------
void gui_set_framebuf(const SceDisplayFrameBuf* pParam)
{
    // Copy display framebuffer parameters (the actual output buffer)
    memcpy(&g_gui_fb, pParam, sizeof(SceDisplayFrameBuf));
    // Calculate scaling ratios from our fixed reference (960x544) to the current display resolution.
    g_gui_fb_w_ratio = pParam->width / 960.0f;
    g_gui_fb_h_ratio = pParam->height / 544.0f;
}

//--------------------------------------------------------------------
// Internal rendering: clear and text drawing into g_gui_buffer
//--------------------------------------------------------------------
void gui_clear(void)
{
    int total = GUI_WIDTH * GUI_HEIGHT;
    for (int i = 0; i < total; i++)
        g_gui_buffer[i] = g_color_bg;
}

// Draw a single character at (x,y) in the internal buffer.
// Assumes the font bitmap is arranged as contiguous 12x24 blocks per ASCII character.
static void draw_char(char c, int x, int y)
{
    // Fast path for space.
    if (c == ' ')
    {
        for (int yy = 0; yy < (int)(g_gui_font_height * g_gui_font_scale); yy++)
        {
            if (y + yy >= GUI_HEIGHT)
                break;
            for (int xx = 0; xx < (int)(g_gui_font_width * g_gui_font_scale); xx++)
            {
                if (x + xx >= GUI_WIDTH)
                    break;
                g_gui_buffer[(y + yy) * GUI_WIDTH + (x + xx)] = g_color_bg;
            }
        }
        return;
    }

    // Calculate bytes per row in the font bitmap.
    int bytes_per_row = (g_gui_font_width + 7) / 8;
    // Compute the offset in the font bitmap for this character.
    int char_offset = ((int)c) * g_gui_font_height * bytes_per_row;

    for (int yy = 0; yy < (int)(g_gui_font_height * g_gui_font_scale); yy++)
    {
        int font_y = yy / g_gui_font_scale;
        if (y + yy >= GUI_HEIGHT)
            break;
        for (int xx = 0; xx < (int)(g_gui_font_width * g_gui_font_scale); xx++)
        {
            int font_x = xx / g_gui_font_scale;
            if (x + xx >= GUI_WIDTH)
                break;
            int           byte_index                      = char_offset + font_y * bytes_per_row + (font_x / 8);
            int           bit_index                       = 7 - (font_x % 8);
            unsigned char byte                            = g_gui_font[byte_index];
            bool          pixel_on                        = ((byte >> bit_index) & 0x1) != 0;
            g_gui_buffer[(y + yy) * GUI_WIDTH + (x + xx)] = pixel_on ? g_color_text : g_color_bg;
        }
    }
}

// Render a null-terminated string at (x,y) into the internal buffer.
void gui_print(int x, int y, const char* str)
{
    while (*str)
    {
        draw_char(*str, x, y);
        x += (int)(g_gui_font_width * g_gui_font_scale);
        str++;
    }
}

void gui_printf(int x, int y, const char* format, ...)
{
    char    buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    gui_print(x, y, buffer);
}

// Check if UI state changed
bool gui_state_changed()
{
    bool current_menu_active        = menu_is_active();
    bool current_console_active     = console_is_active();
    bool current_hex_browser_active = hex_browser_is_active();

    bool changed = (current_menu_active != g_last_menu_active) || (current_console_active != g_last_console_active) ||
                   (current_hex_browser_active != g_last_hexbrowser_active);

    g_last_menu_active       = current_menu_active;
    g_last_console_active    = current_console_active;
    g_last_hexbrowser_active = current_hex_browser_active;

    return changed;
}

// Check if framebuffer resolution changed
bool gui_fb_res_changed()
{
    static int last_width  = 0;
    static int last_height = 0;

    bool changed = (g_gui_fb.width != last_width) || (g_gui_fb.height != last_height);

    last_width  = g_gui_fb.width;
    last_height = g_gui_fb.height;

    return changed;
}

// Inline integer square root (using bit shifting)
static inline int isqrt(int num)
{
    int res = 0;
    int bit = 1 << 30; // The second-to-top bit set

    // "bit" starts at the highest power of four <= num
    while (bit > num)
        bit >>= 2;

    while (bit != 0)
    {
        if (num >= res + bit)
        {
            num -= res + bit;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

void gui_cpy(void)
{
    // const int margin = 10;
    const int margin = 0;

    int scaled_width  = (int)(GUI_WIDTH * g_gui_fb_w_ratio) - (2 * margin);
    int scaled_height = (int)(GUI_HEIGHT * g_gui_fb_h_ratio) - (2 * margin);
    int x_offset      = (g_gui_fb.width - scaled_width) / 2;
    int y_offset      = (g_gui_fb.height - scaled_height) / 2;

    // const int corner_radius    = 16;
    const int corner_radius    = 0;
    const int corner_radius_sq = corner_radius * corner_radius;

    // Buffer for efficient copying
    static rgba_t* line_buffer    = NULL;
    static int     prev_row_width = 0;

    // Allocate or reallocate line buffer if needed
    if (!line_buffer || prev_row_width < scaled_width)
    {
        if (line_buffer)
        {
            ksceKernelFreeMemBlock((SceUID)line_buffer);
        }
        int    size       = (scaled_width * sizeof(rgba_t) + 0xfff) & ~0xfff;
        SceUID buffer_uid = ksceKernelAllocMemBlock("line_buffer", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, size, NULL);
        if (buffer_uid >= 0)
        {
            ksceKernelGetMemBlockBase(buffer_uid, (void**)&line_buffer);
            prev_row_width = scaled_width;
        }
    }

    for (int line = 0; line < scaled_height; line++)
    {
        int src_line_idx = (int)((line + margin) / g_gui_fb_h_ratio);
        if (src_line_idx >= GUI_HEIGHT)
            break;

        int fb_line = y_offset + line;
        if (fb_line >= g_gui_fb.height)
            break;

        int inset_x = margin;

        if (src_line_idx < corner_radius || src_line_idx >= GUI_HEIGHT - corner_radius)
        {
            int delta = corner_radius - (src_line_idx < corner_radius ? src_line_idx : (GUI_HEIGHT - src_line_idx - 1));
            inset_x += corner_radius - isqrt(corner_radius_sq - delta * delta);
        }

        int scaled_x_offset = (int)(inset_x * g_gui_fb_w_ratio);
        int fb_offset       = fb_line * g_gui_fb.pitch + x_offset + scaled_x_offset;
        int row_width       = scaled_width - (scaled_x_offset * 2);

        // Use line buffer if available for more efficient copy
        if (line_buffer)
        {
            // Copy from internal buffer to line buffer first
            memcpy(line_buffer, &g_gui_buffer[src_line_idx * GUI_WIDTH + inset_x], sizeof(rgba_t) * row_width);

            // Then copy from line buffer to framebuffer
            ksceKernelMemcpyKernelToUser((uintptr_t)&((rgba_t*)g_gui_fb.base)[fb_offset], line_buffer,
                                         sizeof(rgba_t) * row_width);
        }
        else
        {
            // Direct copy as fallback
            ksceKernelMemcpyKernelToUser((uintptr_t)&((rgba_t*)g_gui_fb.base)[fb_offset],
                                         &g_gui_buffer[src_line_idx * GUI_WIDTH + inset_x], sizeof(rgba_t) * row_width);
        }
    }
}
