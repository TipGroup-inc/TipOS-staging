/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: disp.h ~ funcoes anotadas: 0
 */
/* ~*~ disp.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef DISP_H
#define DISP_H

// ~~ API do Compositor ~~
// Essas funções são a interface pública pro sistema de janelas~
// Quem quiser desenhar na tela, passa por aqui (não tem atalho, viu?~)

// Cria uma janela na posição (x,y) com tamanho (w,h) e título.
// Retorna o ID da janela ou -1 se estourou o limite.
int disp_create_window(int x, int y, int w, int h, const char *title);

// Atualiza posição do mouse por deslocamento (dx, dy).
// O cursor fica preso nos limites da tela (pq sim~)
void disp_update_mouse(int dx, int dy);

// Renderiza o frame atual: janelas + cursor, tudo bonitinho~
void disp_render(void);

// Inicializa o compositor (cria janela padrão, prepara estado).
// Chama uma vez no boot e pronto~ ☆
void disp_init(void);

#endif



/* ♥ disp.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
