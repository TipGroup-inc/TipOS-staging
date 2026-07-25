/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_label.c ~ funcoes anotadas: 3
 */
/* ~*~ owt_label.c ~ "Label ~ texto parado na tela" ~*~
 * Desenha um texto na tela. So isso. Nao tem evento, nao tem animacao,
 * nao tem nada. É o widget mais simples do OWT. As vezes eu acho que
 * label devia ser so uma funcao e nao um widget inteiro, mas enfim~
 * arquitetura é arquitetura (mesmo quando é overkill). kyun! <3

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_label.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void label_draw(owt_widget_t *w) {
    owt_label_t *lbl = (owt_label_t *)w;
    owt_theme_t *t = owt_theme_get();
    owt_draw_text(w->x, w->y, lbl->text, t->text_primary);
}

/* ~ cuidado que essa aqui morde ~ */
owt_label_t *owt_label_create(const char *text, int x, int y) {
    owt_label_t *lbl = (owt_label_t *)owt_widget_alloc(x, y, 200, 20, sizeof(owt_label_t));
    if (!lbl) return 0;
    int i = 0;
    while (text[i] && i < 255) { lbl->text[i] = text[i]; i++; }
    lbl->text[i] = 0;
    lbl->base.draw = label_draw;
    return lbl;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_label_set_text(owt_label_t *lbl, const char *text) {
    int i = 0;
    while (text[i] && i < 255) { lbl->text[i] = text[i]; i++; }
    lbl->text[i] = 0;
}



/* ♥ owt_label.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
