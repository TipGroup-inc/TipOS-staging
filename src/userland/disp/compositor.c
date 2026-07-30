/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: compositor.c ~ funcoes anotadas: 5
 */
/* ~*~ compositor.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/*
 * Compositor — gerencia janelas e desenha na tela.
 * Por enquanto roda inline no kernel (sem processo separado).
 * Futuro: servidor user-space com IPC.
 */

#include <stdint.h>

#define MAX_WINDOWS 8
#define SCREEN_W 320
#define SCREEN_H 200
#define TITLE_H 12

extern void vga_gfx_putpixel(int x, int y, uint8_t color);
extern void vga_gfx_fillrect(int x, int y, int w, int h, uint8_t color);
extern void vga_gfx_clear(uint8_t color);

typedef struct {
    int x, y, w, h;
    uint8_t focused;
    char title[32];
    int dirty;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count;
static int focused_win = -1;
static int mouse_x = SCREEN_W / 2, mouse_y = SCREEN_H / 2;

/* ~~ Criando coisa nova~~ que emocionante! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int disp_create_window(int x, int y, int w, int h, const char *title) {
    if (win_count >= MAX_WINDOWS) return -1;
    window_t *w2 = &windows[win_count];
    w2->x = x; w2->y = y; w2->w = w; w2->h = h;
    w2->focused = 0;
    w2->dirty = 1;
    int i = 0;
    while (*title && i < 31) w2->title[i++] = *title++;
    w2->title[i] = '\0';
    focused_win = win_count;
    win_count++;
    return win_count - 1;
}

/* ~~ disp_update_mouse ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void disp_update_mouse(int dx, int dy) {
    mouse_x += dx;
    mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void draw_titlebar(window_t *w) {
    uint8_t bg = w->focused ? 0x17 : 0x08;
    int i = 0;
    vga_gfx_fillrect(w->x, w->y, w->w, TITLE_H, bg);
    for (int tx = w->x + 2; tx < w->x + w->w - 2 && w->title[i]; tx++, i++)
        vga_gfx_putpixel(tx, w->y + 2, 0x0F);
    vga_gfx_fillrect(w->x, w->y + TITLE_H, w->w, 1, 0x07);
}

/* ~~ disp_render ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void disp_render(void) {
    vga_gfx_clear(0x01);
    for (int i = 0; i < win_count; i++) {
        window_t *w = &windows[i];
        if (!w->dirty && i != focused_win) continue;
        draw_titlebar(w);
        vga_gfx_fillrect(w->x, w->y + TITLE_H + 1, w->w, w->h - TITLE_H - 1, 0x07);
        w->dirty = 0;
    }
    vga_gfx_fillrect(mouse_x, mouse_y, 7, 7, 0x0F);
    vga_gfx_fillrect(mouse_x + 1, mouse_y + 1, 5, 5, 0x00);
    vga_gfx_putpixel(mouse_x + 3, mouse_y + 3, 0x0F);
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void disp_init(void) {
    win_count = 0;
    disp_create_window(10, 10, 200, 140, "Terminal");
}




/* ♥ compositor.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
