/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_textbox.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_textbox.h ~ "TextBox com struct tag, mesma luta do button" ~*~
 * Mesma historia do button: o typedef sem tag fazia o callback
 * on_enter dar warning de ponteiro incompativel. Coloquei a tag
 * e agora o compilador nao grita mais~ paz e amor! kyun~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_TEXTBOX_H
#define OWT_TEXTBOX_H
#include "owt_base.h"

typedef struct owt_textbox {
    owt_widget_t base;
    char buffer[256];
    int cursor_pos;
    int max_length;
    void (*on_enter)(struct owt_textbox *tb);
} owt_textbox_t;

owt_textbox_t *owt_textbox_create(int x, int y, int w);
void owt_textbox_set_text(owt_textbox_t *tb, const char *text);
const char *owt_textbox_get_text(owt_textbox_t *tb);
void owt_textbox_key(owt_textbox_t *tb, char key);

#endif

/* ♥ owt_textbox.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */
