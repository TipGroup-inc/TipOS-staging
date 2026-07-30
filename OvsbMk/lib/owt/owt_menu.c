/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_menu.c ~ funcoes anotadas: 5
 */
/* ~*~ owt_menu.c ~ "Menu popup ~ igual restaurante japones!" ~*~
 * Um menu que abre quando clica, mostra itens, e chama on_action
 * quando seleciona um. Suporta separadores (linha horizontal) e
 * hover (muda de cor quando passa o mouse). É tipo um context menu
 * do Windows 95 mas sem o efeito 3D~ saudosismo puro! kyun~ <3

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_menu.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void menu_draw(owt_widget_t *w) {
    owt_menu_t *m = (owt_menu_t *)w;
    owt_theme_t *t = owt_theme_get();
    
    owt_draw_rect(w->x, w->y, w->w, 22, t->bg_secondary);
    owt_draw_rect(w->x, w->y + 21, w->w, 1, t->border);
    
    int cx = w->x + 4;
    for (int i = 0; i < m->item_count; i++) {
        if (m->items[i][0] == '-') { cx += 16; continue; }
        int w_text = 0; while (m->items[i][w_text]) w_text++;
        owt_draw_text(cx, w->y + 3, m->items[i], t->text_primary);
        cx += w_text * 8 + 16;
    }
    
    if (m->is_open) {
        owt_draw_rect(w->x, w->y + 22, 150, m->item_count * 22, t->bg_primary);
        for (int i = 0; i < m->item_count; i++) {
            uint32_t bg = (i == m->hover_index) ? t->accent : t->bg_primary;
            owt_draw_rect(w->x, w->y + 22 + i * 22, 150, 22, bg);
            owt_draw_text(w->x + 4, w->y + 25 + i * 22, m->items[i], t->text_primary);
        }
    }
}

/* ~ cuidado que essa aqui morde ~ */
owt_menu_t *owt_menu_create(int x, int y, int w) {
    owt_menu_t *m = (owt_menu_t *)owt_widget_alloc(x, y, w, 22, sizeof(owt_menu_t));
    if (!m) return 0;
    m->item_count = 0;
    m->hover_index = -1;
    m->is_open = 0;
    m->on_action = 0;
    m->base.draw = menu_draw;
    return m;
}

/* ~~ owt_menu_add_item ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void owt_menu_add_item(owt_menu_t *m, const char *label) {
    if (m->item_count >= 16) return;
    int i = 0;
    while (label[i] && i < 63) { m->items[m->item_count][i] = label[i]; i++; }
    m->items[m->item_count][i] = 0;
    m->item_count++;
}

/* ~~ owt_menu_add_separator ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_menu_add_separator(owt_menu_t *m) {
    if (m->item_count >= 16) return;
    m->items[m->item_count][0] = '-';
    m->items[m->item_count][1] = 0;
    m->item_count++;
}

/* ~~ owt_menu_click ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_menu_click(owt_menu_t *m, int mouse_y) {
    if (!m->is_open) {
        m->is_open = 1;
        return;
    }
    int rel_y = mouse_y - m->base.y - 22;
    int index = rel_y / 22;
    if (index >= 0 && index < m->item_count) {
        if (m->on_action) m->on_action(m, index);
    }
    m->is_open = 0;
}



/* ♥ owt_menu.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
