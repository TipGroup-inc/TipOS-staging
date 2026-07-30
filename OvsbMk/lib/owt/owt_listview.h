/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_listview.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_listview.h ~ "ListView com struct tag, tchau warnings!" ~*~
 * Mesma saga: struct anonima + callback = incompatible pointer type.
 * Adicionei a tag struct owt_listview e agora o C nao reclama mais.
 * Quem diria que uma tag resolvia tanta treta~ >_< nyan! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_LISTVIEW_H
#define OWT_LISTVIEW_H
#include "owt_base.h"

typedef struct owt_listview {
    owt_widget_t base;
    char items[32][64];
    int item_count;
    int selected_index;
    int scroll_offset;
    void (*on_select)(struct owt_listview *lv, int index);
} owt_listview_t;

owt_listview_t *owt_listview_create(int x, int y, int w, int h);
void owt_listview_add_item(owt_listview_t *lv, const char *item);
void owt_listview_clear(owt_listview_t *lv);
const char *owt_listview_get_selected(owt_listview_t *lv);
void owt_listview_click(owt_listview_t *lv, int mouse_y);

#endif

/* ♥ owt_listview.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
