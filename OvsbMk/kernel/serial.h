/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: serial.h ~ funcoes anotadas: 0
 */
/* ♥ serial.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ SERIAL_H ~ "Serial: debug sem monitor!"
 * Dica: puts manda string, putc manda char~
 * Baka, usa isso pra debuggar ao invés de print na tela! */
#ifndef SERIAL_H
#define SERIAL_H
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(uint32_t v);
#endif

/* ♥ serial.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */




/* ♥ serial.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
