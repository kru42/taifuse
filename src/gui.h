#ifndef GUI_H
#define GUI_H

#include <vitasdkkern.h>
#include <stdint.h>
#include <stdbool.h>

#define GUI_WIDTH  960
#define GUI_HEIGHT 544

#define FONT_WIDTH         9
#define FONT_HEIGHT        18
#define FONT_BYTES_PER_ROW (((FONT_WIDTH - 1) / 8) + 1)

typedef struct
{
    uint8_t r, g, b, a;
} rgba_t;

// External framebuffer (must be set by display init code)
extern SceDisplayFrameBuf g_gui_fb;

extern bool g_menu_visible;

// GUI initialization and deinitialization
int  gui_init(void);
void gui_deinit(void);

// GUI rendering functions
void gui_clear(void);
void gui_draw_menu_background(void);
void gui_draw_char(char c, int x, int y, rgba_t color);
void gui_print(const char* str, int x, int y, rgba_t color);
void gui_copy(void);

// Input handling
void gui_input_check(uint32_t buttons);

#endif // GUI_H
