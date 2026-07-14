#include <stdint.h>
#include "serial.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(0x3F8 + 1, 0x00); outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03); outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03); outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static int serial_is_transmit_empty(void) { return inb(0x3F8 + 5) & 0x20; }
void serial_putc(char c) { while (!serial_is_transmit_empty()); outb(0x3F8, c); }
void serial_puts(const char *s) { while (*s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); } }
