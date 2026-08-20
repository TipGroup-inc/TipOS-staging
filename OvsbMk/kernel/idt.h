/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: idt.h ~ funcoes anotadas: 0
 */
/* ♥ IDT Header ~ "Leia-me antes de chamar int 0x80!"
 * Dica: O IRQ0 (timer) é vetor 32, IRQ1 (teclado) é 33~
 * Não confunda ou seu PC vai travar~ kyun! */
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);
void idt_set_entry(int num, uint64_t handler, uint16_t selector, uint8_t flags);
void idt_set_syscall(void);
void idt_set_irq1(void);
void idt_set_irq12(void);

#define IDT_PRESENT     0x80
#define IDT_DPL_USER    0x60
#define IDT_INT_GATE    0x0E
#define IDT_TRAP_GATE   0x0F
#define IRQ0  32
#define IRQ1  33

#endif

/* ♥ idt.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */

/* ♥ idt.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
