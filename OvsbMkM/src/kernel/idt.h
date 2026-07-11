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

#define IDT_PRESENT     0x80
#define IDT_DPL_USER    0x60
#define IDT_INT_GATE    0x0E
#define IRQ0  32
#define IRQ1  33

#endif
