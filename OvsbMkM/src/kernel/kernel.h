#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

void vga_puts(const char *s);
void vga_putchar(char c);
void memory_init(void);
void smc_init(void);
void nvram_init(void);

#endif
