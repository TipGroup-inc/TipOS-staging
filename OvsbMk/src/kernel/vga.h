#include "../lib/gui/vesa.h"
#ifndef VGA_H
#define VGA_H
#include <stdint.h>

extern volatile unsigned short *vga;
extern int cx, cy;
extern uint8_t vga_attr;
extern int g_fb_active;
extern framebuffer_t g_fb;

void vga_putchar(char c);
void vga_puts(const char *s);
void vga_clear(void);
void set_vga_color(uint8_t color);
#endif
