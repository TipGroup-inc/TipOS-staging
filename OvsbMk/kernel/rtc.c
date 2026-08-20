/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: rtc.c ~ funcoes anotadas: 5
 */
/* ♥ rtc.c ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ RTC - Real Time Clock ~ "Acorda CMOS, me dá as horas!"
 * Dica: registrador 0x00 = segundos, 0x02 = minutos, 0x04 = horas~
 * 0x07 = dia do mes, 0x08 = mes, 0x09 = ano~
 * O CMOS fala BCD, então a gente converte~ BCD pra decimal~
 * Se der merda, é porque o NMI tá desabilitado~ kyun~ */

#include "rtc.h"

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static uint8_t read_cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int bcd_to_dec(int v) {
    return (v & 0x0F) + ((v >> 4) * 10);
}

/* ♥ Leitura completa do RTC ~ todos os campos de uma vez!
 * Ano = BCD + 2000 ~ porque ainda estamos no seculo 21~
 * Se o CMOS nao responder, é porque o PC é muito velho~ */
/* ~ essa demorou pra debugar, respeita ~ */
void rtc_read(rtc_time_t *t) {
    t->s  = bcd_to_dec(read_cmos(0x00));
    t->m  = bcd_to_dec(read_cmos(0x02));
    t->h  = bcd_to_dec(read_cmos(0x04));
    t->dy = bcd_to_dec(read_cmos(0x07));
    t->mo = bcd_to_dec(read_cmos(0x08));
    t->yr = bcd_to_dec(read_cmos(0x09)) + 2000;
}




/* ♥ rtc.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */
