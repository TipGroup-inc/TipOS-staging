#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

void vga_puts(const char *s);
void vga_putchar(char c);
void memory_init(void);
void smc_init(void);
void nvram_init(void);

typedef struct { int yr, mo, dy, h, m, s; } rtc_time;
void rtc_read(rtc_time *t);

#endif
