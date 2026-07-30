/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_statusbar.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_statusbar.h ~ "Barra de status ~ mostra texto bonitinho!" ~*~
 * Uma barra de status que exibe texto na parte inferior da janela.
 * Simples, sem struct tag pq nao tem callback (por enquanto).
 * Se tiver bug, provavelmente é falta de café~ kyun! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_STATUSBAR_H
#define OWT_STATUSBAR_H
#include "owt_base.h"

typedef struct {
    owt_widget_t base;
    char text[256];
} owt_statusbar_t;

owt_statusbar_t *owt_statusbar_create(int x, int y, int w);
void owt_statusbar_set_text(owt_statusbar_t *sb, const char *text);

#endif



/* ♥ owt_statusbar.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
