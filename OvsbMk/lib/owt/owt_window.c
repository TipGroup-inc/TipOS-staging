/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_window.c ~ funcoes anotadas: 4
 */
/* ~*~ owt_window.c ~ "Janela com titulo, barra, e botao de fechar!" ~*~
 * Implementacao da janela OWT. Desenha a barra de titulo, o botao
 * de fechar (X), e o conteudo interno. A barra de titulo é azul
 * escuro com texto branco (tema dark, obvio~). O botao X fica
 * vermelho quando passa o mouse (igual macOS, mas mais ~gambiarra~).
 * Se clicar no X, a janela fecha (milagre!). kyun~ <3

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_window.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void window_draw(owt_widget_t *w) {
    owt_window_t *win = (owt_window_t *)w;
    owt_theme_t *t = owt_theme_get();
    
    owt_draw_rect(win->base.x, win->base.y, win->base.w, win->base.h, t->bg_primary);
    owt_draw_rect(win->base.x, win->base.y, win->base.w, 24, t->accent);
    owt_draw_text(win->base.x + 8, win->base.y + 4, win->title, t->text_primary);
    
    owt_draw_rect(win->base.x + win->base.w - 20, win->base.y + 4, 16, 16, 0xFFCC2222);
    owt_draw_text(win->base.x + win->base.w - 16, win->base.y + 3, "x", t->text_primary);
    
    if (win->content)
        owt_widget_draw(win->content);
}

/* ~ essa demorou pra debugar, respeita ~ */
owt_window_t *owt_window_create(const char *title, int x, int y, int w, int h) {
    owt_window_t *win = (owt_window_t *)owt_widget_alloc(x, y, w, h, sizeof(owt_window_t));
    if (!win) return 0;
    int i = 0;
    while (title[i] && i < 127) { win->title[i] = title[i]; i++; }
    win->title[i] = 0;
    win->base.draw = window_draw;
    return win;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_window_set_content(owt_window_t *win, owt_widget_t *content) {
    /* ~~ Se passar NULL aqui, o bagulho explode. Entao vou proteger ~~
     * Quem diria que um if salva uma vida (ou um page fault). >_< */
    if (!win || !content) return;
    win->content = content;
    content->parent = (owt_widget_t *)win;
}

/* ~~ owt_window_draw ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_window_draw(owt_window_t *win) {
    owt_widget_draw((owt_widget_t *)win);
}



/* ♥ owt_window.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
