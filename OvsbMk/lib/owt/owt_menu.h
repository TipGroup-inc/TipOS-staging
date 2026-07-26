/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_menu.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_menu.h ~ "Menu com struct tag, ultimo da lista (juro)" ~*~
 * Ok, esse é o ultimo. Mesmo padrao: typedef anonimo + callback = warn.
 * Adicionei struct owt_menu e pronto. Nao vou mentir, demorei 2 horas
 * pra entender pq tava dando warning. Era a tag. Sempre foi a tag. affs~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_MENU_H
#define OWT_MENU_H
#include "owt_base.h"

typedef struct owt_menu {
    owt_widget_t base;
    char items[16][64];
    int item_count;
    int hover_index;
    int is_open;
    void (*on_action)(struct owt_menu *menu, int index);
} owt_menu_t;

owt_menu_t *owt_menu_create(int x, int y, int w);
void owt_menu_add_item(owt_menu_t *menu, const char *label);
void owt_menu_add_separator(owt_menu_t *menu);
void owt_menu_click(owt_menu_t *menu, int mouse_y);

#endif

/* ♥ owt_menu.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
