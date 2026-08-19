/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: pit.h ~ funcoes anotadas: 0
 */
/* ♥ pit.h ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ PIT Header ~ timer_ticks é volatil pq o IRQ0 muda ele!
 * Dica: sempre declare volatile pra variáveis de ISR~
 * Senão o compilador vai otimizar e você vai ler 0 pra sempre~ */
#ifndef PIT_H
#define PIT_H
#include <stdint.h>
extern volatile uint64_t timer_ticks;
void timer_tick_handler(void);
void pit_init(void);
void sleep_ms(uint64_t ms);
#endif

/* ♥ pit.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */




/* ♥ pit.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */
