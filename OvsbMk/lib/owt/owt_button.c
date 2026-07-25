/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_button.c ~ funcoes anotadas: 5
 */
/* ~*~ owt_button.c ~ "Botao que clica (as vezes)!" ~*~
 * Implementacao do botao OWT. Desenha um retangulo com texto dentro,
 * muda de cor quando o mouse passa por cima (hover), e chama o callback
 * on_click quando clica. Nada de mais, mas demorei 3 horas pra acertar
 * o padding do texto. .-. nyan~ <3

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_button.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void button_draw(owt_widget_t *w) {
    owt_button_t *btn = (owt_button_t *)w;
    owt_theme_t *t = owt_theme_get();
    
    uint32_t bg = t->bg_button;
    if (btn->is_pressed) bg = t->bg_button_pressed;
    else if (btn->is_hovered) bg = t->bg_button_hover;
    
    owt_draw_rect(w->x, w->y, w->w, w->h, bg);
    owt_draw_rect(w->x, w->y, w->w, 1, t->border);
    owt_draw_rect(w->x, w->y + w->h - 1, w->w, 1, t->border);
    owt_draw_rect(w->x, w->y, 1, w->h, t->border);
    owt_draw_rect(w->x + w->w - 1, w->y, 1, w->h, t->border);
    
    int text_w = 0;
    for (int i = 0; btn->text[i]; i++) text_w += 8;
    int tx = w->x + (w->w - text_w) / 2;
    int ty = w->y + (w->h - 16) / 2;
    owt_draw_text(tx, ty, btn->text, t->text_primary);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
owt_button_t *owt_button_create(const char *text, int x, int y, int w, int h) {
    owt_button_t *btn = (owt_button_t *)owt_widget_alloc(x, y, w, h, sizeof(owt_button_t));
    if (!btn) return 0;
    int i = 0;
    while (text[i] && i < 127) { btn->text[i] = text[i]; i++; }
    btn->text[i] = 0;
    btn->is_pressed = 0;
    btn->is_hovered = 0;
    btn->on_click = 0;
    btn->base.draw = button_draw;
    return btn;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_button_set_text(owt_button_t *btn, const char *text) {
    int i = 0;
    while (text[i] && i < 127) { btn->text[i] = text[i]; i++; }
    btn->text[i] = 0;
}

/* ~~ owt_button_is_hovered ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int owt_button_is_hovered(owt_button_t *btn, int mx, int my) {
    return (mx >= btn->base.x && mx < btn->base.x + btn->base.w &&
            my >= btn->base.y && my < btn->base.y + btn->base.h);
}

/* ~~ owt_button_click ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_button_click(owt_button_t *btn) {
    if (btn->on_click) btn->on_click(btn);
}



/* ♥ owt_button.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
