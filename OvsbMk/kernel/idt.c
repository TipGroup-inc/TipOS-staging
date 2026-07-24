/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: idt.c ~ funcoes anotadas: 5
 */
/* ♥ IDT - Interrupt Descriptor Table ~ "Tabela dos sonhos!" ♥
 * Se a IDT nao funcionar, suas IRQs vao chorar~ 
 * Cada entrada tem 16 bytes (64-bit mode), nao confunda com a de 8!
 * Agora com syscall gate pra ring 3 ~ int 0x80 liberado!
 * 
 * O idt_handler() despeja os registradores todinhos quando da excecao,
 * incluindo RIP, CS, RFLAGS, e o stack do usuario. Adicionei o dump
 * do scr_w e next_cascade em 0x100034B8/0x100034A0 pra debug do DISP
 * (tava dando divide-by-zero e ninguem sabia pq). kyun~ <3 */
#include "idt.h"
#include "serial.h"
#include "memory.h"

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
extern void irq0(void);
extern void keyboard_irq_handler(void);
extern void mouse_irq_handler(void);
extern void syscall_isr(void);

/* ~ cuidado que essa aqui morde ~ */
void idt_set_entry(int num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
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
    for (int i = 33; i < IDT_ENTRIES; i++) {
        if (i == 128) continue;
        idt_set_entry(i, (uint64_t)irq0, 0x08, IDT_PRESENT | IDT_INT_GATE);
    }
    /* ♥ Syscall gate: trap gate (IF nao desligado) + DPL=3 pra ring 3 */
    idt_set_entry(128, (uint64_t)syscall_isr, 0x08,
                  IDT_PRESENT | IDT_TRAP_GATE | 0x60);
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" :: "m"(idt_ptr));
}

/* ~ essa demorou pra debugar, respeita ~ */
void idt_set_irq1(void) {
    idt_set_entry(IRQ1, (uint64_t)keyboard_irq_handler, 0x08,
                  IDT_PRESENT | IDT_INT_GATE);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void idt_set_irq12(void) {
    idt_set_entry(IRQ0 + 12, (uint64_t)mouse_irq_handler, 0x08,
                  IDT_PRESENT | IDT_INT_GATE);
}

/* ~~ idt_handler ~ "Morreu? Vou te mostrar ONDE!" ~~
 * Quando uma excecao acontece (tipo divisao por zero, page fault, etc),
 * a CPU empilha SS, RSP, RFLAGS, CS, RIP (se veio do ring 3) e o isr_common
 * empilha todos os registradores. Esse handler entao imprime TUDO no serial
 * pra gente poder debugar. Salva a vida (e o sanity) do dev~ <3
 * 
 * Layout do stack no isr_common:
 *   regs[0..14]  = r15..rax  (15 registradores salvos)
 *   regs[15]     = numero da ISR (empilhado pelo ISR_NOERR/ISR_ERR)
 *   regs[16]     = codigo de erro (0 se nao tem erro)
 *   regs[17]     = RIP (empilhado pela CPU)
 *   regs[18]     = CS
 *   regs[19]     = RFLAGS
 *   regs[20]     = RSP (stack do usuario, se veio do ring 3)
 *
 * Adicionei o dump de scr_w e next_cascade pq o DISP tava dando
 * idiv por zero e ninguem sabia pq scr_w=0. Agora a gente sabe~ */
/* ~ cuidado que essa aqui morde ~ */
void idt_handler(uint64_t *regs) {
    uint64_t exc_num = regs[15];
    uint64_t err_code = regs[16];

    if (exc_num == 14) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        if (cr2 != 0xDEADBEEF) {
            serial_puts("\r\nPF@");
            serial_puthex((uint32_t)cr2);
            serial_puts(" err=");
            serial_puthex((uint32_t)err_code);
            serial_puts(" RIP=");
            serial_puthex((uint32_t)regs[17]);
            serial_puts(" RDI=");
            serial_puthex((uint32_t)regs[9]);
            serial_puts(" RSI=");
            serial_puthex((uint32_t)regs[10]);
            serial_puts(" RDX=");
            serial_puthex((uint32_t)regs[11]);
            serial_puts(" RCX=");
            serial_puthex((uint32_t)regs[12]);
            serial_puts(" R8=");
            serial_puthex((uint32_t)regs[7]);
            serial_puts(" R9=");
            serial_puthex((uint32_t)regs[6]);
            serial_puts(" R10=");
            serial_puthex((uint32_t)regs[5]);
            serial_puts(" R15=");
            serial_puthex((uint32_t)regs[0]);
            serial_puts(" R14=");
            serial_puthex((uint32_t)regs[1]);
            serial_puts(" R13=");
            serial_puthex((uint32_t)regs[2]);
            serial_puts(" R12=");
            serial_puthex((uint32_t)regs[3]);
            uint64_t pml4; asm("mov %%cr3, %0" : "=r"(pml4));
            serial_puts(" CR3="); serial_puthex((uint32_t)pml4);
            uint64_t *pml4v = (uint64_t *)(uintptr_t)pml4;
            serial_puts(" P4R0="); serial_puthex((uint32_t)pml4v[0]);
            serial_puts(" stride="); serial_puthex(*(uint32_t*)0x10014510);
            serial_puts(" scr_w="); serial_puthex(*(uint32_t*)0x10014518);
            serial_puts(" scr_h="); serial_puthex(*(uint32_t*)0x10014514);
            serial_puts(" bb="); serial_puthex(*(uint64_t*)0x10014520);
            int pml4_idx = (cr2 >> 39) & 0x1FF;
            int pdpt_idx = (cr2 >> 30) & 0x1FF;
            int pd_idx   = (cr2 >> 21) & 0x1FF;
            serial_puts(" pml4i="); serial_puthex((uint32_t)pml4_idx);
            serial_puts(" pdpti="); serial_puthex((uint32_t)pdpt_idx);
            serial_puts(" pdi="); serial_puthex((uint32_t)pd_idx);
            serial_puts(" PTi="); serial_puthex((uint32_t)((cr2>>12)&0x1FF));
            serial_puts(" P4["); serial_puthex((uint32_t)pml4_idx); serial_puts("]="); serial_puthex((uint32_t)pml4v[pml4_idx]);
            if (pml4v[pml4_idx] & 1) {
                uint64_t pdpt_pa = pml4v[pml4_idx] & ~0xFFFULL;
                uint64_t *pdptv = (uint64_t *)(uintptr_t)pdpt_pa;
                serial_puts(" PPpa="); serial_puthex((uint32_t)pdpt_pa);
                serial_puts(" PP["); serial_puthex((uint32_t)pdpt_idx); serial_puts("]="); serial_puthex((uint32_t)pdptv[pdpt_idx]);
                serial_puts(" PP["); serial_puthex((uint32_t)0); serial_puts("]="); serial_puthex((uint32_t)pdptv[0]);
                if (pdptv[pdpt_idx] & 1) {
                    uint64_t pd_pa = pdptv[pdpt_idx] & ~0xFFFULL;
                    uint64_t *pdv = (uint64_t *)(uintptr_t)pd_pa;
                    serial_puts(" PDpa="); serial_puthex((uint32_t)pd_pa);
                    serial_puts(" PD["); serial_puthex((uint32_t)pd_idx); serial_puts("]="); serial_puthex((uint32_t)pdv[pd_idx]);
                    if ((pdv[pd_idx] & 1) && !(pdv[pd_idx] & 0x80)) {
                        int pt_idx = (cr2 >> 12) & 0x1FF;
                        uint64_t pt_pa = pdv[pd_idx] & ~0xFFFULL;
                        uint64_t *ptv = (uint64_t *)(uintptr_t)pt_pa;
                        serial_puts(" PTpa="); serial_puthex((uint32_t)pt_pa);
                        serial_puts(" PT["); serial_puthex((uint32_t)pt_idx); serial_puts("]="); serial_puthex((uint32_t)ptv[pt_idx]);
                    }
                }
            }
            serial_puts("\r\n");
        }
        return;
    }

    serial_puts("\r\n!!! EXC#");
    serial_puthex((uint32_t)exc_num);
    serial_puts(" err=");
    serial_puthex((uint32_t)err_code);
    serial_puts(" RIP=");
    serial_puthex((uint32_t)regs[17]);
    serial_puts(" R15=");
    serial_puthex((uint32_t)regs[0]);
    serial_puts(" R14=");
    serial_puthex((uint32_t)regs[1]);
    serial_puts(" R13=");
    serial_puthex((uint32_t)regs[2]);
    serial_puts(" R12=");
    serial_puthex((uint32_t)regs[3]);
    serial_puts(" R11=");
    serial_puthex((uint32_t)regs[4]);
    serial_puts(" R10=");
    serial_puthex((uint32_t)regs[5]);
    serial_puts(" R9=");
    serial_puthex((uint32_t)regs[6]);
    serial_puts(" R8=");
    serial_puthex((uint32_t)regs[7]);
    serial_puts(" RBP=");
    serial_puthex((uint32_t)regs[8]);
    serial_puts(" RDI=");
    serial_puthex((uint32_t)regs[9]);
    serial_puts(" RSI=");
    serial_puthex((uint32_t)regs[10]);
    serial_puts(" RDX=");
    serial_puthex((uint32_t)regs[11]);
    serial_puts(" RCX=");
    serial_puthex((uint32_t)regs[12]);
    serial_puts(" RBX=");
    serial_puthex((uint32_t)regs[13]);
    serial_puts(" RAX=");
    serial_puthex((uint32_t)regs[14]);
    serial_puts(" RSP=");
    serial_puthex((uint32_t)regs[20]);
    serial_puts(" CR2=");
    { uint64_t _cr2; __asm__ volatile("mov %%cr2, %0" : "=r"(_cr2)); serial_puthex((uint32_t)_cr2); }
    serial_puts(" !!!\r\n");
    serial_puts(" CODE:");
    uint8_t *code = (uint8_t *)(uintptr_t)regs[17];
    for (int i = 0; i < 16; i++) {
        serial_puthex((uint32_t)code[i]);
        serial_puts(" ");
    }
    serial_puts("\r\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* ♥ idt.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */

/* ♥ idt.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
