/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: tss.c ~ funcoes anotadas: 2
 */
/* ♥ tss.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ TSS ~ Task State Segment pro mode switching fofinho!
 * Dica: sem TSS nao da pra voltar da ring 3~ seu baka!
 * O RSP0 aqui é o stack pointer que a CPU carrega
 * automaticamente quando entra no kernel vindo do user mode~
 * Tipo uma escada rolante~ whee~ kyun! */
#include "tss.h"
#include "memory.h"
#include "serial.h"

extern uint64_t gdt64[];
extern uint64_t gdt_tss_slot[];

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

/* ♥ A variavel `tss` eh usada pelo switch.asm direto! */
volatile uint8_t tss[sizeof(tss_t)] __attribute__((aligned(16)));

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ cuidado que essa aqui morde ~ */
void tss_init(void) {
    /* Zera tudo */
    for (int i = 0; i < (int)sizeof(tss_t); i++)
        tss[i] = 0;

    tss_t *t = (tss_t *)tss;
    t->iomap_base = sizeof(tss_t);

    uint64_t base = (uint64_t)(uintptr_t)tss;
    uint64_t limit = sizeof(tss_t) - 1;

    /* Preenche descritor de TSS na GDT (slots 0x28-0x2F, 16 bytes) */
    uint64_t desc_low = 0;
    desc_low |= (limit & 0xFFFF) << 0;          /* Limit[15:0] */
    desc_low |= ((base >> 24) & 0xFF) << 56;     /* Base[31:24] */
    desc_low |= 0x89ULL << 40;                   /* Present, DPL=0, TSS64 available */
    desc_low |= ((limit >> 16) & 0xF) << 48;     /* Limit[19:16] */
    desc_low |= 0x0ULL << 52;                    /* Flags */
    desc_low |= (base & 0xFFFFFF) << 16;         /* Base[23:16] << 16, Base[15:0] << 16 */

    /* Na verdade vou montar byte a byte pra ter certeza */

    gdt_tss_slot[0] = 0;
    gdt_tss_slot[1] = 0;

    uint8_t *desc = (uint8_t *)&gdt_tss_slot[0];
    /* Byte 0-1: Limit[15:0] */
    desc[0] = limit & 0xFF;
    desc[1] = (limit >> 8) & 0xFF;
    /* Byte 2-3: Base[15:0] */
    desc[2] = base & 0xFF;
    desc[3] = (base >> 8) & 0xFF;
    /* Byte 4: Base[23:16] */
    desc[4] = (base >> 16) & 0xFF;
    /* Byte 5: Type=1001 (TSS64 available), System=0, DPL=00, Present=1 */
    desc[5] = 0x89;
    /* Byte 6: Limit[19:16]=0, Flags=0000 */
    desc[6] = (limit >> 16) & 0x0F;
    /* Byte 7: Base[31:24] */
    desc[7] = (base >> 24) & 0xFF;
    /* Byte 8-11: Base[63:32] */
    desc = (uint8_t *)&gdt_tss_slot[1];
    desc[0] = (base >> 32) & 0xFF;
    desc[1] = (base >> 40) & 0xFF;
    desc[2] = (base >> 48) & 0xFF;
    desc[3] = (base >> 56) & 0xFF;
    /* Byte 12-15: Reserved */
    desc[4] = 0;
    desc[5] = 0;
    desc[6] = 0;
    desc[7] = 0;

    __asm__ volatile("ltr %%ax" :: "a"(0x28));
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ cuidado que essa aqui morde ~ */
void tss_set_rsp0(uint64_t rsp0) {
    tss_t *t = (tss_t *)tss;
    t->rsp0 = rsp0;
}

/* ♥ TSS ~ Task State Segment! ring 3 precisa de RSP0 pra syscall~ */

/* ♥ tss.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */




/* ♥ tss.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
