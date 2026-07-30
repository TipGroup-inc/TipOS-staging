/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_combobox.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_combobox.h ~ "Combobox com struct tag, mais um pro grupo" ~*~
 * Adivinha? Mesma treta. Typedef sem tag, callback dando warning.
 * Coloquei struct owt_combobox e fim de papo. O compilador agradece,
 * eu agradeco, e o codigo fica mais bonito~ kyun kyun! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_COMBOBOX_H
#define OWT_COMBOBOX_H
#include "owt_base.h"

typedef struct owt_combobox {
    owt_widget_t base;
    char items[16][64];
    int item_count;
    int selected_index;
    int is_open;
    void (*on_select)(struct owt_combobox *cb, int index);
} owt_combobox_t;

owt_combobox_t *owt_combobox_create(int x, int y, int w);
void owt_combobox_add_item(owt_combobox_t *cb, const char *item);
const char *owt_combobox_get_selected(owt_combobox_t *cb);
void owt_combobox_click(owt_combobox_t *cb, int mouse_y);

#endif

/* ♥ owt_combobox.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
