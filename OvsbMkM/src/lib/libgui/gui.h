#ifndef GUI_H
#define GUI_H

#include "vesa.h"

void gui_init(framebuffer_t *fb);
void gui_fill_screen(uint32_t color);
void gui_draw_pixel(int x, int y, uint32_t color);
void gui_draw_rect(int x, int y, int w, int h, uint32_t color);
void gui_draw_char(int x, int y, char c, uint32_t color);
void gui_draw_text(int x, int y, const char *text, uint32_t color);

#endif
