#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include "colors.h"

void vga_puts(const char *s);
void vga_putchar(char c);
void set_vga_color(uint8_t color);
extern uint8_t vga_attr;
void memory_init(void);
void smc_init(void);
void nvram_init(void);

typedef struct { int yr, mo, dy, h, m, s; } rtc_time;
void rtc_read(rtc_time *t);

#endif
