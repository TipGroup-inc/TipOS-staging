#include "idt.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

void pic_init(void) {
    // Salvar máscaras
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Inicializar PIC1 e PIC2
    outb(PIC1_COMMAND, 0x11);  // ICW1: Initialization
    outb(PIC2_COMMAND, 0x11);
    
    outb(PIC1_DATA, 0x20);     // ICW2: Vetor inicial (32)
    outb(PIC2_DATA, 0x28);     // Vetor inicial (40)
    
    outb(PIC1_DATA, 0x04);     // ICW3: PIC2 no IRQ2
    outb(PIC2_DATA, 0x02);
    
    outb(PIC1_DATA, 0x01);     // ICW4: modo 8086
    outb(PIC2_DATA, 0x01);
    
    // Restaurar máscaras
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
