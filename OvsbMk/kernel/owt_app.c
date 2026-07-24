/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: owt_app.c ~ funcoes anotadas: 1
 */
/* ♥ owt_app.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include <stdint.h>
#include "../lib/wm/wm.h"
#include "../lib/owt/owt.h"
#include "console.h"
#include "serial.h"

/* ~~ owt_demo ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void owt_demo(void) {
    console_write("OWT Demo!\n");
    serial_puts("[OWT] Demo OWT!\r\n");

    owt_draw_rect(0, 0, wm_get_scr_w(), wm_get_scr_h(), THEME_BG);

    owt_window_t *win = owt_window_create("OWT Demo", 50, 50, 400, 300);
    owt_label_t *lbl = owt_label_create("Bem-vindo ao OWT!", 80, 40);
    owt_button_t *btn = owt_button_create("Clique Aqui", 120, 100, 150, 35);
    owt_textbox_t *input = owt_textbox_create(50, 160, 300);
    owt_statusbar_t *sb = owt_statusbar_create(0, 280, 400);

    owt_window_set_content(win, (owt_widget_t *)lbl);
    owt_widget_add_child((owt_widget_t *)win, (owt_widget_t *)btn);
    owt_widget_add_child((owt_widget_t *)win, (owt_widget_t *)input);
    owt_widget_add_child((owt_widget_t *)win, (owt_widget_t *)sb);

    owt_window_draw(win);
    wm_flush();
}
/* ♥ OWT Demo ~ testa os widgets todos! bota pra quebrar~ */




/* ♥ owt_app.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
