// demo.c — exemplo mínimo de uso da libdisp
// Compila com: gcc -ffreestanding -nostdlib ... (mesmas flags do TipOS)

#include "libdisp.h"
#include <stdint.h>

static void write_str(const char *s) {
    while (*s) {
        __asm__ volatile ("int $0x80" : : "a"(4ULL), "b"(1ULL), "c"(s), "d"(1ULL) : "memory");
        s++;
    }
}

void _start(void) {
    uint64_t fb_addr;
    uint32_t w, h, pitch;

    if (libdisp_init(&fb_addr, &w, &h, &pitch) < 0) {
        write_str("no framebuffer\n");
        __asm__ volatile ("int $0x80" : : "a"(1ULL), "b"(1ULL));
    }

    // Allocate backbuffer
    uint64_t bb = 0;
    __asm__ volatile ("int $0x80"
        : "=a"(bb)
        : "a"(197ULL), "b"(0ULL), "c"((uint64_t)w * h * 4), "d"(3ULL), "S"(0x1002ULL)
        : "memory");
    uint32_t *back = (uint32_t *)(uintptr_t)bb;
    if (!back || back == (uint32_t*)-1) {
        write_str("alloc failed\n");
        __asm__ volatile ("int $0x80" : : "a"(1ULL), "b"(1ULL));
    }

    // Fill with blue
    for (uint32_t i = 0; i < w * h; i++)
        back[i] = 0x00001030;

    // Draw a white rectangle
    for (uint32_t y = 100; y < 200; y++)
        for (uint32_t x = 100; x < 300; x++)
            back[y * (pitch / 4) + x] = 0x00FFFFFF;

    libdisp_flush(back);

    // Wait for a key
    char ch = 0;
    __asm__ volatile ("int $0x80" : "=a"(*(uint64_t*)&ch) : "a"(3ULL), "b"(0ULL), "c"(&ch), "d"(1ULL) : "memory");

    // Exit
    __asm__ volatile ("int $0x80" : : "a"(1ULL), "b"(0ULL));
}
