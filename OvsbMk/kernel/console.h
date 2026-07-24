/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: console.h ~ funcoes anotadas: 0
 */
/* ♥ console.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>

void console_init(void);
void console_putchar(char c);
void console_write(const char *s);
void console_printf(const char *fmt, ...);
void console_clear(void);
void console_show_cursor(int show);
void console_set_fg(uint32_t color);
void console_set_bg(uint32_t color);
void console_set_output_buffer(char *buf, int *len, int max);

#endif

/* ♥ console.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */




/* ♥ console.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
