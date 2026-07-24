#include <stdint.h>
#include "idt.h"
#include "memory.h"
#include "vga.h"
#include "pit.h"
#include "key.h"
#include "env.h"
#include "serial.h"
#include "process.h"
#include "ring3.h"
#include "../drivers/ata.h"
#include "../drivers/mouse.h"
#include "../lib/gui/vesa.h"
#include "shell_cmds.h"
#include "../fs/fat32.h"

void keyboard_init(void);
void pic_init(void);
void tss_init(void);

extern void fb_reset(void);
extern void disp_init(void);
extern void disp_render(void);
extern void disp_update_mouse(int dx, int dy);
extern void disp_handle_click(void);
extern int  disp_drag_state(void);
extern void disp_drag_move(int dx, int dy);

extern char keyboard_read(void);
extern int  keyboard_avail(void);

volatile int sigint_pending = 0;

static void parse_multiboot2(uint32_t magic, uint32_t mb_info) {
    framebuffer_t fb = {0};
    extern framebuffer_t g_fb;
    extern int g_fb_active;
    uint32_t *tags = (uint32_t*)(uintptr_t)mb_info;
    if (magic == 0x36D76289 && tags) {
        uint32_t total = tags[0];
        for (uint32_t i = 8; i < total; ) {
            uint32_t type = *(uint32_t*)((uint8_t*)tags + i);
            uint32_t size = *(uint32_t*)((uint8_t*)tags + i + 4);
            if (type == 8) {
                fb.addr = *(uint64_t*)((uint8_t*)tags + i + 8);
                fb.pitch = *(uint32_t*)((uint8_t*)tags + i + 16);
                fb.width = *(uint32_t*)((uint8_t*)tags + i + 20);
                fb.height = *(uint32_t*)((uint8_t*)tags + i + 24);
                fb.bpp = *(uint8_t*)((uint8_t*)tags + i + 28);
            }
            if (type == 0) break;
            i += size; if (i % 8) i += 8 - (i % 8);
        }
    }
    if (fb.addr && fb.bpp == 32) { g_fb = fb; vesa_init(&g_fb); g_fb_active = 1; fb_reset(); }
}

static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1, %0":"=a"(r):"Nd"(p)); return r; }

extern void rust_entry(void);

static void boot_selector(void) {
    serial_init();
    vga_clear();
    set_vga_color(0x0F);
    vga_puts("TipOS Boot Selector\n");
    vga_puts("Press [R] for Rust memory manager\n");
    vga_puts("Press [C] for C memory manager\n");
    vga_puts("Default: C in 5s...\n");
    for (volatile int i = 0; i < 50000000; i++) {
        if (keyboard_avail()) {
            char c = keyboard_read();
            if (c == 'r' || c == 'R') {
                vga_puts("[Rust] selected\n");
                serial_puts("[Rust] selected\n");
                rust_entry();
                return;
            }
            if (c == 'c' || c == 'C') {
                vga_puts("[C] selected\n");
                serial_puts("[C] selected\n");
                return;
            }
        }
    }
    vga_puts("[C] timeout\n");
    serial_puts("[C] timeout\n");
}

void kmain(uint32_t magic, uint32_t mb_info) {
    idt_init(); pic_init();
    idt_set_syscall(); idt_set_irq1(); idt_set_irq12();
    keyboard_init(); pit_init(); memory_init(); proc_init(); tss_init();
    __asm__ volatile ("sti");

    parse_multiboot2(magic, mb_info);
    if (g_fb_active) vesa_flush();

    boot_selector();
    if (g_fb_active) vesa_flush();

    if (g_fb_active) mouse_init();

    ata_init();
    if (fat32_init() != 0) {
        set_vga_color(0x04); vga_puts("FAT32 FAIL\n"); set_vga_color(0x07);
    }

    if (g_fb_active) disp_init();

    vga_clear();
    set_vga_color(0x0F);
    vga_puts("TipOS OvsbMk\n");
    set_vga_color(0x07);
    if (g_fb_active) vesa_flush();

    shell_loop();

    int frame = 0;
    while (1) {
        /* Drena o buffer 0x60 roteando por AUXB (bit 5 de 0x64): só bytes do
         * mouse alimentam o decodificador; bytes de teclado são descartados
         * aqui (evita corromper o estado do mouse com scancodes). */
        uint8_t st;
        while ((st = inb(0x64)) & 0x01) {
            uint8_t data = inb(0x60);
            if (st & 0x20) mouse_process_byte(data);
        }

        if (mouse_has_moved()) {
            int dx = mouse_get_dx();
            int dy = mouse_get_dy();
            if (disp_drag_state()) disp_drag_move(dx, dy);
            else disp_update_mouse(dx, dy);
        }
        static int last_btn = 0;
        int btn = mouse_get_buttons();
        if (btn && !last_btn) disp_handle_click();
        if (!btn && last_btn) disp_handle_click();
        last_btn = btn;

        if (++frame >= 2) { if (g_fb_active) disp_render(); frame = 0; }

        __asm__ volatile ("hlt");
    }
}
