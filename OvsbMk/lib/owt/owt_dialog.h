/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_dialog.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_dialog.h ~ "Dialogos ~ pergunta se o usuario tem certeza!" ~*~
 * Funcoes pra criar dialogos modais: message (so avisa) e confirm
 * (pergunta sim/nao). Centraliza automaticamente na tela usando
 * wm_get_scr_w/h() - sem hardcode de 1024x768! kyun~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_DIALOG_H
#define OWT_DIALOG_H

void owt_dialog_message(const char *title, const char *message);
int owt_dialog_confirm(const char *title, const char *message);

#endif



/* ♥ owt_dialog.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
