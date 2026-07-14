#include "disp_api.h"
#include "vesa.h"
#include "memory.h"
#include "process.h"
#include <stdint.h>

// ─── sys_disp_get_fb() ────────────────────────────────────────
// Syscall 200: Mapeia o framebuffer VESA no processo atual e
// retorna o endereço virtual + dimensões.
//
// Entrada:
//   a1 = uint64_t* addr    (user space, output)
//   a2 = uint32_t* width   (user space, output)
//   a3 = uint32_t* height  (user space, output)
//   a4 = uint32_t* pitch   (user space, output)
//
// Retorno: 0 sucesso, -1 erro

uint64_t sys_disp_get_fb(uint64_t *addr, uint32_t *width,
                          uint32_t *height, uint32_t *pitch) {
    extern int g_fb_active;
    extern framebuffer_t g_fb;

    if (!g_fb_active || g_fb.bpp != 32)
        return -1;

    // Endereço virtual onde o framebuffer será mapeado no user space
    // Usamos um endereço alto fixo, longe do código userland (0x2000000)
    uint64_t user_fb_va = 0xFFFFFFFF80000000ULL;

    // Mapeia o framebuffer físico no PML4 do processo atual com User bit
    uint64_t fb_size = (uint64_t)g_fb.pitch * g_fb.height;
    fb_size = (fb_size + 0x1FFFFF) & ~0x1FFFFF;  // align 2MB

    pcb_t *current = &proc_table[current_pid];
    int ret = pml4_map_phys(current->pml4, user_fb_va, g_fb.addr, fb_size, 1);
    if (ret < 0) return -1;

    // Escreve os valores de retorno no espaço do usuário
    // (o kernel roda no mesmo espaço de endereçamento que o user durante a syscall)
    if (addr)   *addr   = user_fb_va;
    if (width)  *width  = g_fb.width;
    if (height) *height = g_fb.height;
    if (pitch)  *pitch  = g_fb.pitch;

    return 0;
}

// ─── sys_disp_flush() ─────────────────────────────────────────
// Syscall 201: Copia o backbuffer do user space para o framebuffer
// físico real. O kernel faz a cópia porque o endereço físico do
// framebuffer está mapeado apenas no kernel.
//
// Entrada:
//   a1 = void* backbuffer  (user space pointer para uint32_t[w*h])

void sys_disp_flush(void *backbuffer) {
    extern int g_fb_active;
    extern framebuffer_t g_fb;

    if (!g_fb_active || !backbuffer)
        return;

    uint32_t *fb_phys = (uint32_t *)(uintptr_t)g_fb.addr;
    size_t pixels = (size_t)g_fb.pitch * g_fb.height / 4;

    // fast_memcpy32 com rep movsd — muito mais rápido que loop per-pixel
    __asm__ volatile (
        "rep movsl"
        : "+D"(fb_phys), "+S"(backbuffer), "+c"(pixels)
        :
        : "memory"
    );
}
