/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: rtc.h ~ funcoes anotadas: 0
 */
/* ♥ rtc.h ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef RTC_H
#define RTC_H
#include <stdint.h>

/* ♥ RTC - Real Time Clock ~ "Que horas são? Pergunta pro CMOS!"
 * Dica: porta 0x70 registrador, 0x71 dados ~ BCD mode!
 * O rtc_read() devolve tudo bonitinho na struct~
 * Ano, mes, dia, hora, minuto, segundo~ tudo BCD convertido! */

typedef struct {
    uint16_t yr;
    uint8_t  mo, dy, h, m, s;
} rtc_time_t;

void rtc_read(rtc_time_t *t);

#endif




/* ♥ rtc.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
