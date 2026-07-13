#include <stdint.h>
#include "vesa.h"

#define MAX_WINDOWS 8
#define TITLE_H 16

static int scr_w = 1024;
static int scr_h = 768;
static int gfx_mode = 0; // 0=VGA legacy, 1=VESA

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
static int mouse_x, mouse_y;

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

void disp_update_mouse(int dx, int dy) {
    mouse_x += dx; mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= scr_w) mouse_x = scr_w - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= scr_h) mouse_y = scr_h - 1;
}

static void draw_titlebar(window_t *w) {
    uint32_t bg = 0x00333399;
    if (w->focused) bg = 0x000000CC;
    if (gfx_mode) {
        vesa_draw_rect(w->x, w->y, w->w, TITLE_H, bg);
        int i = 0;
        for (int tx = w->x + 4; tx < w->x + w->w - 8 && w->title[i]; tx += 8, i++)
            vesa_draw_char(tx, w->y + 2, w->title[i], 0x00FFFFFF);
        vesa_draw_rect(w->x, w->y + TITLE_H, w->w, 2, 0x00666666);
    } else {
        uint8_t bg8 = w->focused ? 0x17 : 0x08;
        vga_gfx_fillrect(w->x, w->y, w->w, TITLE_H, bg8);
        int i = 0;
        for (int tx = w->x + 2; tx < w->x + w->w - 2 && w->title[i]; tx++, i++)
            vga_gfx_putpixel(tx, w->y + 2, 0x0F);
        vga_gfx_fillrect(w->x, w->y + TITLE_H, w->w, 1, 0x07);
    }
}

static void draw_client(window_t *w) {
    uint32_t bg = 0x00EEEEEE;
    if (gfx_mode) {
        vesa_draw_rect(w->x, w->y + TITLE_H + 2, w->w, w->h - TITLE_H - 2, bg);
    } else {
        vga_gfx_fillrect(w->x, w->y + TITLE_H + 1, w->w, w->h - TITLE_H - 1, 0x07);
    }
}

void disp_render(void) {
    if (gfx_mode) {
        vesa_fill_screen(0x00224466);
    } else {
        vga_gfx_clear(0x01);
    }
    for (int i = 0; i < win_count; i++) {
        window_t *w = &windows[i];
        if (!w->dirty && i != focused_win) continue;
        draw_titlebar(w);
        draw_client(w);
        w->dirty = 0;
    }
    if (gfx_mode) {
        vesa_draw_rect(mouse_x, mouse_y, 8, 8, 0x00FFFFFF);
        vesa_draw_rect(mouse_x + 2, mouse_y + 2, 4, 4, 0x00000000);
    } else {
        vga_gfx_fillrect(mouse_x, mouse_y, 7, 7, 0x0F);
        vga_gfx_fillrect(mouse_x + 1, mouse_y + 1, 5, 5, 0x00);
        vga_gfx_putpixel(mouse_x + 3, mouse_y + 3, 0x0F);
    }
}

void disp_init(void) {
    win_count = 0;
    focused_win = -1;
    extern int g_fb_active;
    if (g_fb_active) {
        extern framebuffer_t g_fb;
        scr_w = (int)g_fb.width;
        scr_h = (int)g_fb.height;
        gfx_mode = 1;
    } else {
        scr_w = 320;
        scr_h = 200;
        gfx_mode = 0;
    }
    mouse_x = scr_w / 2;
    mouse_y = scr_h / 2;
    disp_create_window(scr_w / 5, scr_h / 6, scr_w * 3 / 5, scr_h * 2 / 3, "Terminal");
}
