/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: pic.c ~ funcoes anotadas: 3
 */
/* ♥ pic.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ PIC - Programmable Interrupt Controller ~ "Mestre dos IRQs!"
 * Dica: PIC1 (0x20) manda IRQs 0-7, PIC2 (0xA0) manda 8-15~
 * Remapeamos pra 32-39 e 40-47 pra não colidir com exceções!
 * Se esquecer de remapear, int 8 vai ser double fault e não timer~ */

#include "idt.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ~ cuidado que essa aqui morde ~ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
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

    // Garantir que IRQ2 (cascade do slave) esteja desmascarada no master
    outb(PIC1_DATA, inb(PIC1_DATA) & ~0x04);
}




/* ♥ pic.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
