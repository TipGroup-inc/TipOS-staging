#ifndef TUI_H
#define TUI_H
#include <stdint.h>

static inline void tui_putchar(char c) {
    __asm__ volatile (
        "mov $4, %%rax\n\t"
        "mov $1, %%rdi\n\t"
        "lea %0, %%rsi\n\t"
        "mov $1, %%rdx\n\t"
        "int $0x80"
        :: "m"(c) : "rax", "rdi", "rsi", "rdx", "rcx", "r11");
}

static inline void tui_puts(const char *s) {
    int len = 0;
    while (s[len]) len++;
    __asm__ volatile (
        "mov $4, %%rax\n\t"
        "mov $1, %%rdi\n\t"
        "mov %0, %%rsi\n\t"
        "mov %1, %%rdx\n\t"
        "int $0x80"
        :: "r"(s), "r"((long)len) : "rax", "rdi", "rsi", "rdx", "rcx", "r11");
}

static inline char tui_getchar(void) {
    char c;
    __asm__ volatile (
        "mov $3, %%rax\n\t"
        "mov $0, %%rdi\n\t"
        "lea %0, %%rsi\n\t"
        "mov $1, %%rdx\n\t"
        "int $0x80"
        : "=m"(c) :: "rax", "rdi", "rsi", "rdx", "rcx", "r11");
    return c;
}

static inline void tui_gotoxy(int x, int y) {
    unsigned short pos = y * 80 + x;
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"((uint16_t)0x3D5));
}

static inline void tui_clear(void) {
    volatile unsigned short *vga = (volatile unsigned short *)0xB8000;
    for (int i = 0; i < 80 * 25; i++) vga[i] = (0x0A << 8) | ' ';
    tui_gotoxy(0, 0);
}

static inline void tui_printnum(int n) {
    char buf[12], *p = buf;
    if (n < 0) { tui_putchar('-'); n = -n; }
    do { *p++ = '0' + (n % 10); n /= 10; } while (n);
    while (p > buf) tui_putchar(*--p);
}

#endif
