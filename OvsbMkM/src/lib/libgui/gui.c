#include "gui.h"
#include "vesa.h"

void gui_init(framebuffer_t *fb) { vesa_init(fb); }
void gui_fill_screen(uint32_t color) { vesa_fill_screen(color); }
void gui_draw_pixel(int x, int y, uint32_t color) { vesa_draw_pixel(x, y, color); }
void gui_draw_rect(int x, int y, int w, int h, uint32_t color) { vesa_draw_rect(x, y, w, h, color); }
void gui_draw_char(int x, int y, char c, uint32_t color) { vesa_draw_char(x, y, c, color); }
void gui_draw_text(int x, int y, const char *text, uint32_t color) { vesa_draw_text(x, y, text, color); }
