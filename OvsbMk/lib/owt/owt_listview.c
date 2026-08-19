/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_listview.c ~ funcoes anotadas: 6
 */
/* ~*~ owt_listview.c ~ "Lista de itens ~ igual Windows 3.11!" ~*~
 * Uma listview que mostra itens numa lista vertical. Dá pra selecionar
 * um item e ele fica destacado (azul, porque é a cor padrao de selecao
 * desde sempre). Tem scroll se tiver mais itens que o espaço. Se voce
 * clicar num item, chama on_select. É tipo ListBox do VB6~ que saudades!
 * kyun~ <3

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_listview.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ cuidado que essa aqui morde ~ */
static void listview_draw(owt_widget_t *w) {
    owt_listview_t *lv = (owt_listview_t *)w;
    owt_theme_t *t = owt_theme_get();
    
    owt_draw_rect(w->x, w->y, w->w, w->h, t->bg_secondary);
    
    int visible = (w->h - 4) / 20;
    for (int i = 0; i < visible && (i + lv->scroll_offset) < lv->item_count; i++) {
        int idx = i + lv->scroll_offset;
        int y = w->y + 2 + i * 20;
        
        if (idx == lv->selected_index) {
            owt_draw_rect(w->x + 2, y, w->w - 4, 18, t->accent);
            owt_draw_text(w->x + 6, y + 2, lv->items[idx], t->text_primary);
        } else {
            owt_draw_text(w->x + 6, y + 2, lv->items[idx], t->text_secondary);
        }
    }
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
owt_listview_t *owt_listview_create(int x, int y, int w, int h) {
    owt_listview_t *lv = (owt_listview_t *)owt_widget_alloc(x, y, w, h, sizeof(owt_listview_t));
    if (!lv) return 0;
    lv->item_count = 0;
    lv->selected_index = -1;
    lv->scroll_offset = 0;
    lv->on_select = 0;
    lv->base.draw = listview_draw;
    return lv;
}

/* ~~ owt_listview_add_item ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_listview_add_item(owt_listview_t *lv, const char *item) {
    if (lv->item_count >= 32) return;
    int i = 0;
    while (item[i] && i < 63) { lv->items[lv->item_count][i] = item[i]; i++; }
    lv->items[lv->item_count][i] = 0;
    lv->item_count++;
}

/* ~~ owt_listview_clear ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_listview_clear(owt_listview_t *lv) {
    lv->item_count = 0;
    lv->selected_index = -1;
}

/* ~ essa demorou pra debugar, respeita ~ */
const char *owt_listview_get_selected(owt_listview_t *lv) {
    if (lv->selected_index >= 0 && lv->selected_index < lv->item_count)
        return lv->items[lv->selected_index];
    return 0;
}

/* ~~ owt_listview_click ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_listview_click(owt_listview_t *lv, int mouse_y) {
    int rel_y = mouse_y - lv->base.y - 2;
    int index = rel_y / 20 + lv->scroll_offset;
    if (index >= 0 && index < lv->item_count) {
        lv->selected_index = index;
        if (lv->on_select) lv->on_select(lv, index);
    }
}



/* ♥ owt_listview.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */
