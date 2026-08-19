/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_window.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_window.h ~ "Janela OWT com titulo e conteudo!" ~*~
 * Uma janela que tem titulo, pode ter botao de fechar e um widget
 * de conteudo interno. Usa owt_widget_t como base (heranca via
 * composicao, porque C nao tem class~ #F). Se quebrar, foi mal! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_WINDOW_H
#define OWT_WINDOW_H
#include "owt_base.h"

typedef struct {
    owt_widget_t base;
    char title[128];
    int can_close;
    owt_widget_t *content;
} owt_window_t;

owt_window_t *owt_window_create(const char *title, int x, int y, int w, int h);
void owt_window_set_content(owt_window_t *win, owt_widget_t *content);
void owt_window_draw(owt_window_t *win);

#endif



/* ♥ owt_window.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */
