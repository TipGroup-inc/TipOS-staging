/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_textbox.c ~ funcoes anotadas: 5
 */
/* ~*~ owt_textbox.c ~ "Caixa de texto ~ digita ai!" ~*~
 * Implementacao da caixa de texto. O usuario digita e aparece na tela~
 * (igual magica!). Suporta backspace, cursor piscante (so que nao, 
 * eu nao implementei o blink ainda), e callback on_enter quando
 * aperta Enter. É tipo um input do HTML mas sem JavaScript! >_<

 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_textbox.h"
#include "owt_draw.h"
#include "owt_theme.h"

/* ~ cuidado que essa aqui morde ~ */
static void textbox_draw(owt_widget_t *w) {
    owt_textbox_t *tb = (owt_textbox_t *)w;
    owt_theme_t *t = owt_theme_get();
    
    owt_draw_rect(w->x, w->y, w->w, 24, t->bg_secondary);
    owt_draw_rect(w->x, w->y, w->w, 1, t->border);
    owt_draw_rect(w->x, w->y + 23, w->w, 1, t->border);
    owt_draw_rect(w->x, w->y, 1, 24, t->border);
    owt_draw_rect(w->x + w->w - 1, w->y, 1, 24, t->border);
    owt_draw_text(w->x + 4, w->y + 4, tb->buffer, t->text_primary);
    owt_draw_rect(w->x + 4 + tb->cursor_pos * 8, w->y + 4, 8, 16, t->accent);
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
owt_textbox_t *owt_textbox_create(int x, int y, int w) {
    owt_textbox_t *tb = (owt_textbox_t *)owt_widget_alloc(x, y, w, 24, sizeof(owt_textbox_t));
    if (!tb) return 0;
    tb->buffer[0] = 0;
    tb->cursor_pos = 0;
    tb->max_length = 30;
    tb->on_enter = 0;
    tb->base.draw = textbox_draw;
    return tb;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_textbox_set_text(owt_textbox_t *tb, const char *text) {
    int i = 0;
    while (text[i] && i < 255) { tb->buffer[i] = text[i]; i++; }
    tb->buffer[i] = 0;
    tb->cursor_pos = i;
}

/* ~ cuidado que essa aqui morde ~ */
const char *owt_textbox_get_text(owt_textbox_t *tb) {
    return tb->buffer;
}

/* ~~ owt_textbox_key ~~ */
/* ~ cuidado que essa aqui morde ~ */
void owt_textbox_key(owt_textbox_t *tb, char key) {
    if (key == '\b') {
        if (tb->cursor_pos > 0) {
            for (int i = tb->cursor_pos; i < 256; i++) tb->buffer[i-1] = tb->buffer[i];
            tb->cursor_pos--;
        }
    } else if (key == '\n') {
        if (tb->on_enter) tb->on_enter(tb);
    } else if (key >= ' ') {
        int len = 0; while (tb->buffer[len]) len++;
        if (len < tb->max_length) {
            for (int i = len + 1; i > tb->cursor_pos; i--) tb->buffer[i] = tb->buffer[i-1];
            tb->buffer[tb->cursor_pos] = key;
            tb->cursor_pos++;
        }
    }
}



/* ♥ owt_textbox.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
