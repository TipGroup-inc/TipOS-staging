/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_base.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_base.h ~ Widget base do OWT ~ "A mae de todos os widgets!" ~*~
 * Define a estrutura base owt_widget_t que todos os outros widgets
 * (button, textbox, listview, combobox, menu) herdam via composicao.
 * Tem callback de draw, on_click e on_key pra cada widget poder se
 * comportar de forma diferente sem precisar de switch gigante.
 * O parent/children permite criar arvores de widgets (sim, é tipo
 * uma DOM tree mas sem JavaScript. Sorte a nossa~) kyun! <3
 *
 * NOTA: O array de filhos children[16] agora é dentro da struct (nao
 * é mais um ponteiro pra array estatica global). Antes tava todo mundo
 * compartilhando o mesmo vetor, era um caos~ arrumei! rssrsrs >_<
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_BASE_H
#define OWT_BASE_H
#include <stdint.h>
#include <stddef.h>

#define OWT_MAX_CHILDREN 16

typedef struct owt_widget {
    int x, y, w, h;
    int visible;
    int enabled;
    struct owt_widget *parent;
    struct owt_widget *children[OWT_MAX_CHILDREN];
    int child_count;
    void (*draw)(struct owt_widget *self);
    void (*on_click)(struct owt_widget *self, int x, int y);
    void (*on_key)(struct owt_widget *self, char key);
} owt_widget_t;

/* ~~ Cria um widget com tamanho especifico (nao só o base!) ~~
 * O parametro size é o tamanho REAL do subtipo (button, textbox, etc).
 * Antes usava sempre sizeof(owt_widget_t) e os cast explodiam tudo.
 * Agora cada create passa sizeof(owt_button_t) e a alocacao é certa.
 * Se vc passar o tamanho errado, a culpa é 100% sua~ >_< */
void *owt_widget_alloc(int x, int y, int w, int h, size_t size);
void owt_widget_draw(owt_widget_t *w);
void owt_widget_add_child(owt_widget_t *parent, owt_widget_t *child);

#endif



/* ♥ owt_base.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
