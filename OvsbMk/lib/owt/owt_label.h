/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_label.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_label.h ~ "Label ~ texto estatico que nao faz nada" ~*~
 * Um label que mostra texto. Só isso. Nao clica, nao digita, nao faz
 * nada alem de existir e ser bonito. É tipo eu no final do semestre~
 * Se quebrar um label, desiste e vai dormir~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_LABEL_H
#define OWT_LABEL_H
#include "owt_base.h"

typedef struct {
    owt_widget_t base;
    char text[256];
} owt_label_t;

owt_label_t *owt_label_create(const char *text, int x, int y);
void owt_label_set_text(owt_label_t *lbl, const char *text);

#endif



/* ♥ owt_label.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
