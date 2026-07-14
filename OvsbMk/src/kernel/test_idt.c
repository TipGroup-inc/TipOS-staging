#include <stdint.h>

static uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static void print_hex(volatile unsigned short *vga, int off, uint64_t val) {
    for (int i = 15; i >= 0; i--) {
        int nib = (val >> (i * 4)) & 0xF;
        vga[off++] = (0x0C << 8) | (nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

void idt_handler(uint64_t *frame) {
    int num = (int)frame[15];
    int err = (int)frame[16];
    volatile unsigned short *vga = (unsigned short *)0xB8000;

    if (num == 33) {
        vga[0] = (0x0E << 8) | 'K';
    } else if (num == 32) {
        vga[1] = (0x0A << 8) | 'T';
    } else if (num < 32) {
        vga[0] = (0x0C << 8) | 'E';
        vga[1] = (0x0C << 8) | ('0' + (num / 10 % 10));
        vga[2] = (0x0C << 8) | ('0' + (num % 10));
        vga[3] = (0x0C << 8) | ':';
        vga[4] = (0x0C << 8) | 'E';
        vga[5] = (0x0C << 8) | 'R';
        vga[6] = (0x0C << 8) | 'R';
        vga[7] = (0x0C << 8) | '=';
        print_hex(vga, 8, err);
        if (num == 14) {
            vga[16] = (0x0C << 8) | 'C';
            vga[17] = (0x0C << 8) | 'R';
            vga[18] = (0x0C << 8) | '2';
            vga[19] = (0x0C << 8) | '=';
            print_hex(vga, 20, read_cr2());
        }
        for (;;) __asm__ volatile("hlt");
    }
}
