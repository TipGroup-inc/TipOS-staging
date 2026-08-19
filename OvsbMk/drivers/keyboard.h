/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: keyboard.h ~ funcoes anotadas: 0
 */
/* ♥ keyboard.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read(void);
/* ~~ keyboard_avail ~~ */
int keyboard_avail(void); /* ♥ nombre consistente com a implementação, seu baka! */
extern volatile int shift_pressed;
extern volatile int ctrl_pressed;

#endif




/* ♥ keyboard.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
