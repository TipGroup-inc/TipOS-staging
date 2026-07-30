/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_base.c ~ funcoes anotadas: 3
 */
/* ~*~ owt_base.c ~ "Widget base ~ a mae de todos!" ~*~
 * Implementacao do widget base: cria, desenha, adiciona filhos.
 * O draw percorre a arvore de widgets recursivamente (sem stack overflow
 * por enquanto~ torce pra nao ter 1000 widgets aninhados!). kyun! <3
 *
 * NOTA IMPORTANTE: Antes usava static pool (owt_widget_t widgets[64])
 * que alocava sizeof(owt_widget_t) pra todo mundo e os subtipos
 * estouravam a memoria pq sao maiores. Arrumei: agora usa kmalloc
 * com o tamanho real do subtipo. E o array de filhos kids era um
 * ponteiro pra static global (todo mundo compartilhava o mesmo vetor,
 * se vc tinha 2 botoes cada um via os filhos do outro HAHAHA).
 * Agora o array children[16] fica dentro de cada widget~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include "owt_base.h"
#include "../kernel/memory.h"

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *owt_widget_alloc(int x, int y, int w, int h, size_t size) {
    /* ~~ Aloca memoria para o subtipo (nao só o base!) ~~
     * Passa o tamanho REAL da subclasse (button, textbox, etc).
     * Se vc passar sizeof(owt_widget_t) ainda funciona pq o
     * tamanho minimo é o da base. Mas nao seja burrao~ >_< */
    owt_widget_t *widget = (owt_widget_t *)kmalloc(size);
    if (!widget) return 0;
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->visible = 1;
    widget->enabled = 1;
    widget->child_count = 0;
    widget->draw = 0;
    widget->on_click = 0;
    widget->on_key = 0;
    widget->parent = 0;
    return widget;
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ cuidado que essa aqui morde ~ */
void owt_widget_draw(owt_widget_t *w) {
    if (!w || !w->visible) return;
    if (w->draw) w->draw(w);
    for (int i = 0; i < w->child_count; i++)
        owt_widget_draw(w->children[i]);
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void owt_widget_add_child(owt_widget_t *parent, owt_widget_t *child) {
    if (parent->child_count < OWT_MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
        child->parent = parent;
    }
}



/* ♥ owt_base.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
