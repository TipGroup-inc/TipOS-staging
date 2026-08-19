/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: pit.c ~ funcoes anotadas: 4
 */
/* ♥ pit.c ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ PIT ~ timer IRQ0! schedule e tempo do sistema~ */
/* ♥ PIT - Programmable Interval Timer ~ "Tick, tick, tick!"
 * Dica: divisor = 1193182 / freq_desejada ~
 * Se dividir por 0, o timer vai explodir (e o PC também)~ lol
 * 100Hz = 1193182/100 = 11931 = 0x2E9B ♥
 * Agora chama schedule() a cada tick! Round-robin ativado~ kyun! */

#include "pit.h"
#include "process.h"
#include "serial.h"

static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port)); }

volatile uint64_t timer_ticks = 0;

/* ~~ Vai tratar esse evento~ se prepara pro caos! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void timer_tick_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20);
    schedule();
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void pit_init(void) {
    uint32_t div = 1193182 / 100;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, (div >> 8) & 0xFF);
    /* Desmascarar IRQ0 no PIC master */
    outb(0x21, inb(0x21) & ~0x01);
}

/* ~~ sleep_ms ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void sleep_ms(uint64_t ms) {
    uint64_t target = timer_ticks + ms / 10 + 1;
    while (timer_ticks < target) { __asm__ volatile ("pause"); }
}

/* ♥ pit.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */




/* ♥ pit.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
