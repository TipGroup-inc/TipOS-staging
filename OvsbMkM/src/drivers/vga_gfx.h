#ifndef VGA_GFX_H
#define VGA_GFX_H
#include <stdint.h>
void vga_gfx_init(void);
void vga_gfx_putpixel(int x, int y, uint8_t color);
void vga_gfx_fillrect(int x, int y, int w, int h, uint8_t color);
void vga_gfx_drawchar(int x, int y, char c, uint8_t fg, uint8_t bg);
void vga_gfx_clear(uint8_t color);
void vga_gfx_restore_text(void);
#endif