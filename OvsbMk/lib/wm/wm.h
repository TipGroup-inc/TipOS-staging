/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ window manager ~ quem manda nas janelinha aqui sou eu!
 * arquivo: wm.h ~ funcoes anotadas: 0
 */
/* ~*~ wm.h ~ Window Manager pro TipOS ~ "Desenhando janelinhas desde 2024!" ~*~
 * Fiz modulo nv no window manager, n vou entrar em detalhes mas ta em lib ne
 * wm_init() inicializa, wm_get_backbuf/stride/scr_w/scr_h() sao os getter
 * e wm_flush() joga o backbuffer pro VESA (tava tudo desalocado, insuportavel)
 * Agora usa wm_get_scr_w/h() inves de 1024x768 hardcoded~ finalmente! >_<
 * Se quebrar... foi mal, foi a cafeina~ kyun! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef WM_H
#define WM_H
#include <stdint.h>

void wm_init(uint32_t *fb, int w, int h, int stride);
uint32_t *wm_get_backbuf(void);
uint32_t wm_get_stride(void);
int wm_get_scr_w(void);
int wm_get_scr_h(void);
void wm_flush(void);

#endif



/* ♥ wm.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
