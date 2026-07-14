#ifndef LIBDISP_H
#define LIBDISP_H

#include <stdint.h>

// ─── Syscall numbers ──────────────────────────────────────────
// Esses números são o CONTRATO entre disp-wm e o kernel.
// O kernel TipOS reservou 200 e 201 para as syscalls de display.

#define SYS_disp_get_fb  200
#define SYS_disp_flush   201

// ─── libdisp_init() ───────────────────────────────────────────
// Syscall 200: Mapeia o framebuffer VESA no processo e retorna
// suas dimensões.
//
// Uso:
//   uint64_t fb_addr;
//   uint32_t w, h, pitch;
//   if (libdisp_init(&fb_addr, &w, &h, &pitch) < 0) { erro; }
//   uint32_t *fb = (uint32_t *)(uintptr_t)fb_addr;
//
// Retorna 0 em sucesso, -1 se não há framebuffer.

static inline int libdisp_init(uint64_t *addr, uint32_t *width,
                                uint32_t *height, uint32_t *pitch) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret), "+b"(pitch), "+c"(height), "+d"(width), "+S"(addr)
        : "a"((uint64_t)SYS_disp_get_fb)
        : "memory"
    );
    return ret;
}

// ─── libdisp_flush() ──────────────────────────────────────────
// Syscall 201: Copia o backbuffer do WM para o framebuffer real.
// O kernel faz a cópia porque o framebuffer físico só está
// mapeado no espaço do kernel (ring 0).
//
// Uso:
//   libdisp_flush(backbuffer);  // backbuffer = uint32_t[w * h]

static inline void libdisp_flush(void *backbuffer) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"((uint64_t)SYS_disp_flush), "b"((uint64_t)(uintptr_t)backbuffer)
        : "memory"
    );
}

#endif
