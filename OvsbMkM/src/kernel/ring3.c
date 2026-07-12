#include "ring3.h"
#include <stdint.h>

extern char gdt_tss_slot[16];
extern char stack_top[];

volatile tss_t tss __attribute__((section(".bss"))) = {0};
volatile uint64_t ring3_exit_rsp_saved = 0;

void tss_init(void) {
    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(tss_t) - 1;

    tss.rsp0 = (uint64_t)&stack_top - 1024;

    gdt_tss_slot[0] = limit & 0xFF;
    gdt_tss_slot[1] = (limit >> 8) & 0xFF;
    gdt_tss_slot[2] = base & 0xFF;
    gdt_tss_slot[3] = (base >> 8) & 0xFF;
    gdt_tss_slot[4] = (base >> 16) & 0xFF;
    gdt_tss_slot[5] = 0x89;
    gdt_tss_slot[6] = 0x40 | ((limit >> 16) & 0x0F);
    gdt_tss_slot[7] = (base >> 24) & 0xFF;
    *(uint32_t *)(gdt_tss_slot + 8) = (base >> 32) & 0xFFFFFFFF;
    *(uint32_t *)(gdt_tss_slot + 12) = 0;

    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));
}

void __attribute__((naked)) enter_ring3(void *entry, void *user_rsp) {
    __asm__ volatile (
        "mov %%rsp, ring3_exit_rsp_saved\n"
        "push $0x23\n"
        "push %1\n"
        "push $0x202\n"
        "push $0x1B\n"
        "push %0\n"
        "xor %%eax, %%eax\n"
        "xor %%ebx, %%ebx\n"
        "xor %%ecx, %%ecx\n"
        "xor %%edx, %%edx\n"
        "xor %%esi, %%esi\n"
        "xor %%edi, %%edi\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "xor %%r10, %%r10\n"
        "xor %%r11, %%r11\n"
        "xor %%r12, %%r12\n"
        "xor %%r13, %%r13\n"
        "xor %%r14, %%r14\n"
        "xor %%r15, %%r15\n"
        "xor %%rbp, %%rbp\n"
        "iretq\n"
        : : "r"(entry), "r"(user_rsp) : "memory"
    );
}
