#include "idt.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void); extern void irq1(void);
extern void syscall_handler_entry(void);
extern void keyboard_irq_handler(void);

void idt_set_entry(int num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

void idt_init(void) {
    uint64_t handlers[] = {
        (uint64_t)isr0,  (uint64_t)isr1,  (uint64_t)isr2,  (uint64_t)isr3,
        (uint64_t)isr4,  (uint64_t)isr5,  (uint64_t)isr6,  (uint64_t)isr7,
        (uint64_t)isr8,  (uint64_t)isr9,  (uint64_t)isr10, (uint64_t)isr11,
        (uint64_t)isr12, (uint64_t)isr13, (uint64_t)isr14, (uint64_t)isr15,
        (uint64_t)isr16, (uint64_t)isr17, (uint64_t)isr18, (uint64_t)isr19,
        (uint64_t)isr20, (uint64_t)isr21, (uint64_t)isr22, (uint64_t)isr23,
        (uint64_t)isr24, (uint64_t)isr25, (uint64_t)isr26, (uint64_t)isr27,
        (uint64_t)isr28, (uint64_t)isr29, (uint64_t)isr30, (uint64_t)isr31
    };
    for (int i = 0; i < 32; i++)
        idt_set_entry(i, handlers[i], 0x08, IDT_PRESENT | IDT_INT_GATE);
    idt_set_entry(IRQ0, (uint64_t)irq0, 0x08, IDT_PRESENT | IDT_INT_GATE);
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" :: "m"(idt_ptr));
}

void idt_set_syscall(void) {
    idt_set_entry(0x80, (uint64_t)syscall_handler_entry, 0x08, 
                  IDT_PRESENT | IDT_DPL_USER | IDT_INT_GATE);
}

void idt_set_irq1(void) {
    idt_set_entry(IRQ1, (uint64_t)keyboard_irq_handler, 0x08, 
                  IDT_PRESENT | IDT_INT_GATE);
}
