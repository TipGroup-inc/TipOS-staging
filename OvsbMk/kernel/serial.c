/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: serial.c ~ funcoes anotadas: 4
 */
/* ♥ serial.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ SERIAL ~ saida serial COM1! debug sem monitor~ */
/* ♥ SERIAL - COM1 Serial ~ "Comunicação serial? Que vintage~"
 * Dica: 0x3F8 é a porta padrão da COM1!
 * Se não funcionar, verifica se o QEMU tem -serial stdio~ kyun! */
#include <stdint.h>
#include "serial.h"

/* ~ essa demorou pra debugar, respeita ~ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
/* ~ cuidado que essa aqui morde ~ */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ cuidado que essa aqui morde ~ */
void serial_init(void) {
    outb(0x3F8 + 1, 0x00); outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03); outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03); outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static int serial_is_transmit_empty(void) { return inb(0x3F8 + 5) & 0x20; }
/* ~~ serial_putc ~~ */
void serial_putc(char c) { while (!serial_is_transmit_empty()); outb(0x3F8, c); }
/* ~~ serial_puts ~~ */
void serial_puts(const char *s) { while (*s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); } }
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void serial_puthex(uint32_t v) {
    static const char hex[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_putc(hex[(v >> i) & 0xF]);
    }
}

/* ♥ serial.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */




/* ♥ serial.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
