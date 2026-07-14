#include "parse_mb2.h"

framebuffer_t g_fb_parsed = {0};

void parse_multiboot2(uint32_t magic, uint32_t addr, framebuffer_t *fb) {
    if (magic != 0x36D76289) return;
    uint8_t *base = (uint8_t*)(uintptr_t)addr;
    uint32_t total = *(uint32_t*)base;
    if (total < 16) return;
    for (uint32_t i = 8; i + 8 < total; ) {
        uint32_t t = *(uint32_t*)(base + i);
        uint32_t s = *(uint32_t*)(base + i + 4);
        if (s < 8) break;
        if (t == 8 && s >= 32) {
            fb->addr   = *(uint64_t*)(base + i + 8);
            fb->pitch  = *(uint32_t*)(base + i + 16);
            fb->width  = *(uint32_t*)(base + i + 20);
            fb->height = *(uint32_t*)(base + i + 24);
            fb->bpp    = *(uint8_t *)(base + i + 28);
        }
        if (s > 4096) break;
        if (s % 8) s += 8 - (s % 8);
        i += s;
    }
}
