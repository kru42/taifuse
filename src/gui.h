#ifndef _GUI_H_
#define _GUI_H_

#include <vitasdkkern.h>
#include <stdint.h>

// GUI dimensions (fixed internal resolution)
#define GUI_WIDTH  960
#define GUI_HEIGHT 544

// Font dimensions
#define GUI_FONT_W 8
#define GUI_FONT_H 8

// RGBA color structure
typedef union
{
    struct
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    } rgba;
    uint32_t uint32;
} rgba_t;

static const rgba_t DEFAULT_TEXT_COLOR      = {.rgba = {255, 255, 255, 255}};
static const rgba_t DEFAULT_BG_COLOR        = {.rgba = {0, 0, 128, 255}};
static const rgba_t DEFAULT_HIGHLIGHT_COLOR = {.rgba = {0, 255, 255, 255}};

// Rendering API (rendering only; no overclocking, profiling, etc.)
int  gui_init(void);
void gui_deinit(void);
void gui_set_framebuf(const SceDisplayFrameBuf* pParam);
void gui_input_check(SceCtrlButtons buttons);
void gui_clear(void);
void gui_print(int x, int y, const char* str);
void gui_printf(int x, int y, const char* format, ...);
void gui_cpy(void);

#endif
