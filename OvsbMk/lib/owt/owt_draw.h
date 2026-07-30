/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_draw.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_draw.h ~ "Desenho basico: pixel, rect, texto!" ~*~
 * Funcoes de desenho do OWT. Tudo vai pro backbuffer (wm_get_backbuf())
 * e usa wm_get_scr_w/h() pra saber os limites da tela (sem hardcode!).
 * Antes usava extern, agora da #include no wm.h~ mais elegante! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_DRAW_H
#define OWT_DRAW_H
#include <stdint.h>
void owt_draw_pixel(int x, int y, uint32_t color);
void owt_draw_rect(int x, int y, int w, int h, uint32_t color);
void owt_draw_text(int x, int y, const char *text, uint32_t color);
void owt_draw_char(int x, int y, char c, uint32_t color);
#endif



/* ♥ owt_draw.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
