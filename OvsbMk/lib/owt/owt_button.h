/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_button.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_button.h ~ "Botao com struct tag pq o callback tava dando warn" ~*~
 * Arrumei um struct tag pro callback, pq o tipo anonimo tava dando
 * "incompatible pointer type" toda vez que usava o on_click. Agora tem
 * struct owt_button pra chamar de seu~ >_< nyan! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_BUTTON_H
#define OWT_BUTTON_H
#include "owt_base.h"

typedef struct owt_button {
    owt_widget_t base;
    char text[128];
    int is_pressed;
    int is_hovered;
    void (*on_click)(struct owt_button *btn);
} owt_button_t;

owt_button_t *owt_button_create(const char *text, int x, int y, int w, int h);
void owt_button_set_text(owt_button_t *btn, const char *text);
int owt_button_is_hovered(owt_button_t *btn, int mouse_x, int mouse_y);
void owt_button_click(owt_button_t *btn);

#endif

/* ♥ owt_button.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
