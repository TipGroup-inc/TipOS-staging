/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_statusbar.c ~ funcoes anotadas: 3
 */
/* ~*~ owt_statusbar.c ~ "Barra de status ~ mostra texto embaixo!" ~*~
 * Uma barra na parte inferior que mostra texto (tipo "Pronto" ou
 * "Carregando..."). Nao faz nada alem de exibir texto, mas é util
 * pra dar feedback pro usuario. É tipo o status bar do Visual Basic
 * 6 mas sem a opçao de painel~ simplicidade é tudo! >_<

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_statusbar.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void statusbar_draw(owt_widget_t *w) {
    owt_statusbar_t *sb = (owt_statusbar_t *)w;
    owt_theme_t *t = owt_theme_get();
    owt_draw_rect(w->x, w->y, w->w, 20, t->bg_secondary);
    owt_draw_rect(w->x, w->y, w->w, 1, t->border);
    owt_draw_text(w->x + 4, w->y + 3, sb->text, t->text_secondary);
}

/* ~ essa demorou pra debugar, respeita ~ */
owt_statusbar_t *owt_statusbar_create(int x, int y, int w) {
    owt_statusbar_t *sb = (owt_statusbar_t *)owt_widget_alloc(x, y, w, 20, sizeof(owt_statusbar_t));
    if (!sb) return 0;
    sb->text[0] = 'P'; sb->text[1] = 'r'; sb->text[2] = 'o'; sb->text[3] = 'n'; sb->text[4] = 't'; sb->text[5] = 'o'; sb->text[6] = 0;
    sb->base.draw = statusbar_draw;
    return sb;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ essa demorou pra debugar, respeita ~ */
void owt_statusbar_set_text(owt_statusbar_t *sb, const char *text) {
    int i = 0;
    while (text[i] && i < 255) { sb->text[i] = text[i]; i++; }
    sb->text[i] = 0;
}



/* ♥ owt_statusbar.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
