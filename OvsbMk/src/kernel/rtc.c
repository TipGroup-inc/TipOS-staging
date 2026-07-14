#include <stdint.h>
#include "rtc.h"

static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }

static uint8_t read_cmos(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
static int cmos_bcd(int v) { return (v & 0x0F) + ((v / 16) * 10); }

void rtc_read(rtc_time *t) {
    t->s = cmos_bcd(read_cmos(0x00)); t->m = cmos_bcd(read_cmos(0x02));
    t->h = cmos_bcd(read_cmos(0x04)); t->dy = cmos_bcd(read_cmos(0x07));
    t->mo = cmos_bcd(read_cmos(0x08)); t->yr = cmos_bcd(read_cmos(0x09)) + 2000;
}
