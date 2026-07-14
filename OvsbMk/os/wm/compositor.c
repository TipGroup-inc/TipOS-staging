#include <stdint.h>
#include <stddef.h>
#include "../../lib/gui/vesa.h"
#include "../../kernel/memory.h"

#define MAX_WINDOWS 8
#define TITLE_H 16
#define CLOSE_W 14
#define PANEL_H 24
#define NEW_BTN_W 20

static int scr_w = 1024;
static int scr_h = 768;
static int gfx_mode = 0;

extern void vga_gfx_putpixel(int x, int y, uint8_t color);
extern void vga_gfx_fillrect(int x, int y, int w, int h, uint8_t color);
extern void vga_gfx_clear(uint8_t color);

typedef struct {
    int x, y, w, h;
    uint8_t focused;
    char title[32];
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count;
static int focused_win = -1;
static int mouse_x, mouse_y;
static int old_mx = -1, old_my = -1;

enum { DRAG_NONE, DRAG_MOVE };
static int drag_state = DRAG_NONE;
static int drag_win = -1;
static int drag_off_x, drag_off_y;

static int next_cascade = 30;

int disp_create_window(int x, int y, int w, int h, const char *title) {
    if (win_count >= MAX_WINDOWS) return -1;
    window_t *w2 = &windows[win_count];
    w2->x = x; w2->y = y; w2->w = w; w2->h = h;
    w2->focused = 0;
    int i = 0;
    while (*title && i < 31) w2->title[i++] = *title++;
    w2->title[i] = '\0';
    focused_win = win_count;
    win_count++;
    return win_count - 1;
}

int disp_new_window(const char *title) {
    int w = scr_w * 3 / 5;
    int h = scr_h * 2 / 3 - PANEL_H;
    if (h < 100) h = 100;
    int x = next_cascade;
    int y = next_cascade;
    next_cascade = (next_cascade + 30) % (scr_w / 2);
    if (x + w > scr_w) x = 0;
    if (y + h > scr_h - PANEL_H) y = 0;
    return disp_create_window(x, y, w, h, title);
}

void disp_update_mouse(int dx, int dy) {
    mouse_x += dx; mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= scr_w) mouse_x = scr_w - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= scr_h) mouse_y = scr_h - 1;
}

static void raise_window(int idx) {
    if (idx < 0 || idx >= win_count || idx == win_count - 1) return;
    window_t tmp = windows[idx];
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    windows[win_count - 1] = tmp;
    focused_win = win_count - 1;
    windows[focused_win].focused = 1;
}

static void close_window(int idx) {
    if (idx < 0 || idx >= win_count) return;
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    win_count--;
    focused_win = win_count - 1;
    if (focused_win >= 0) windows[focused_win].focused = 1;
}

enum { HIT_NONE, HIT_TITLE, HIT_CLOSE, HIT_CLIENT, HIT_PANEL, HIT_NEW_BTN };

static int hit_test(int mx, int my, int *out_idx) {
    if (my >= scr_h - PANEL_H) {
        *out_idx = -1;
        if (mx >= scr_w - NEW_BTN_W - 4) return HIT_NEW_BTN;
        return HIT_PANEL;
    }
    for (int i = win_count - 1; i >= 0; i--) {
        window_t *w = &windows[i];
        if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h)
            continue;
        *out_idx = i;
        if (my < w->y + TITLE_H) {
            if (mx >= w->x + w->w - CLOSE_W) return HIT_CLOSE;
            return HIT_TITLE;
        }
        return HIT_CLIENT;
    }
    return HIT_NONE;
}

static int panel_btn_hit(int mx, int *out_win) {
    int x = 4;
    for (int i = 0; i < win_count; i++) {
        int bw = 8;
        for (int j = 0; windows[i].title[j]; j++) bw += 8;
        if (bw > 200) bw = 200;
        if (mx >= x && mx < x + bw) { *out_win = i; return 1; }
        x += bw + 4;
    }
    return 0;
}

int disp_drag_state(void) { return drag_state; }
int disp_drag_win(void) { return drag_win; }
void disp_drag_move(int dx, int dy) {
    if (drag_win < 0 || drag_win >= win_count) return;
    window_t *w = &windows[drag_win];
    w->x += dx; w->y += dy;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->x + w->w > scr_w) w->x = scr_w - w->w;
    if (w->y + w->h > scr_h - PANEL_H) w->y = scr_h - PANEL_H - w->h;
    disp_update_mouse(dx, dy);
}

void disp_handle_click(void) {
    if (drag_state == DRAG_MOVE) {
        drag_state = DRAG_NONE;
        drag_win = -1;
        return;
    }
    int wi;
    int ht = hit_test(mouse_x, mouse_y, &wi);
    if (ht == HIT_CLOSE) {
        close_window(wi);
        return;
    }
    if (ht == HIT_NEW_BTN) {
        disp_new_window("Terminal");
        return;
    }
    if (ht == HIT_TITLE) {
        raise_window(wi);
        drag_state = DRAG_MOVE;
        drag_win = wi;
        drag_off_x = mouse_x - windows[wi].x;
        drag_off_y = mouse_y - windows[wi].y;
        return;
    }
    if (ht == HIT_CLIENT) {
        raise_window(wi);
        return;
    }
    if (ht == HIT_PANEL) {
        int pwi;
        if (panel_btn_hit(mouse_x, &pwi)) {
            raise_window(pwi);
        }
        return;
    }
}

void disp_close_focused(void) {
    if (focused_win >= 0) close_window(focused_win);
}

void disp_cycle_focus(void) {
    if (win_count <= 1) return;
    int idx = focused_win;
    if (idx < 0) idx = 0;
    int next = (idx + 1) % win_count;
    windows[idx].focused = 0;
    windows[next].focused = 1;
    focused_win = next;
}

static void draw_titlebar(window_t *w) {
    uint32_t bg = w->focused ? 0x000044AA : 0x00223366;
    if (gfx_mode) {
        vesa_draw_rect(w->x, w->y, w->w - CLOSE_W, TITLE_H, bg);
        vesa_draw_rect(w->x + w->w - CLOSE_W, w->y, CLOSE_W, TITLE_H, 0x00AA2222);
        vesa_draw_char(w->x + w->w - 11, w->y + 2, 'x', 0x00FFFFFF);
        int i = 0;
        for (int tx = w->x + 4; tx < w->x + w->w - CLOSE_W - 4 && w->title[i]; tx += 8, i++)
            vesa_draw_char(tx, w->y + 2, w->title[i], 0x00FFFFFF);
        vesa_draw_rect(w->x, w->y + TITLE_H, w->w, 2, 0x006688CC);
    } else {
        uint8_t bg8 = w->focused ? 0x17 : 0x08;
        vga_gfx_fillrect(w->x, w->y, w->w, TITLE_H, bg8);
        vga_gfx_fillrect(w->x + w->w - CLOSE_W, w->y, CLOSE_W, TITLE_H, 0x04);
        int i = 0;
        for (int tx = w->x + 2; tx < w->x + w->w - CLOSE_W - 2 && w->title[i]; tx++, i++)
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

static void draw_panel(void) {
    uint32_t bar = 0x001A1A2E;
    uint32_t border = 0x00333366;
    vesa_draw_rect(0, scr_h - PANEL_H, scr_w, PANEL_H, bar);
    vesa_draw_rect(0, scr_h - PANEL_H, scr_w, 1, border);
    int x = 4;
    for (int i = 0; i < win_count; i++) {
        window_t *w = &windows[i];
        int bw = 8;
        for (int j = 0; w->title[j]; j++) bw += 8;
        if (bw > 200) bw = 200;
        uint32_t btn = (i == focused_win) ? 0x00335599 : 0x00223355;
        vesa_draw_rect(x, scr_h - PANEL_H + 3, bw, PANEL_H - 6, btn);
        vesa_draw_rect(x, scr_h - PANEL_H + 3, bw, 1, 0x004466AA);
        int tx = x + 4;
        int ti = 0;
        for (; tx < x + bw - 4 && w->title[ti]; tx += 8, ti++)
            vesa_draw_char(tx, scr_h - PANEL_H + 6, w->title[ti], 0x00CCCCCC);
        x += bw + 4;
    }
    int bx = scr_w - NEW_BTN_W - 4;
    vesa_draw_rect(bx, scr_h - PANEL_H + 3, NEW_BTN_W, PANEL_H - 6, 0x00225533);
    vesa_draw_char(bx + 6, scr_h - PANEL_H + 5, '+', 0x00AAFFAA);
}

static uint32_t cur_bg[8][8];
static uint32_t stride;
static uint32_t *backbuf;

static void cur_save(int mx, int my) {
    for (int y = 0; y < 8 && my + y < scr_h; y++)
        for (int x = 0; x < 8 && mx + x < scr_w; x++)
            cur_bg[y][x] = backbuf[(my + y) * stride + (mx + x)];
}

static void cur_restore(int mx, int my) {
    for (int y = 0; y < 8 && my + y < scr_h; y++)
        for (int x = 0; x < 8 && mx + x < scr_w; x++)
            backbuf[(my + y) * stride + (mx + x)] = cur_bg[y][x];
}

static void cur_draw(int mx, int my) {
    vesa_draw_rect(mx, my, 8, 8, 0x00FFFFFF);
    vesa_draw_rect(mx + 2, my + 2, 4, 4, 0x00000000);
}

void disp_render(void) {
    if (!gfx_mode) {
        vga_gfx_clear(0x01);
        for (int i = 0; i < win_count; i++) {
            draw_titlebar(&windows[i]);
            draw_client(&windows[i]);
        }
        vga_gfx_fillrect(mouse_x, mouse_y, 7, 7, 0x0F);
        vga_gfx_fillrect(mouse_x + 1, mouse_y + 1, 5, 5, 0x00);
        vga_gfx_putpixel(mouse_x + 3, mouse_y + 3, 0x0F);
        return;
    }
    if (old_mx >= 0) cur_restore(old_mx, old_my);
    vesa_fill_screen(0x00001030);
    for (int i = 0; i < win_count; i++) {
        draw_titlebar(&windows[i]);
        draw_client(&windows[i]);
    }
    draw_panel();
    cur_save(mouse_x, mouse_y);
    cur_draw(mouse_x, mouse_y);
    vesa_flush();
    old_mx = mouse_x;
    old_my = mouse_y;
}

int disp_win_count(void) { return win_count; }

void disp_init(void) {
    win_count = 0;
    focused_win = -1;
    drag_state = DRAG_NONE;
    drag_win = -1;
    old_mx = -1;
    next_cascade = 30;
    extern int g_fb_active;
    extern framebuffer_t g_fb;
    if (g_fb_active) {
        scr_w = (int)g_fb.width;
        scr_h = (int)g_fb.height;
        gfx_mode = 1;
    } else {
        scr_w = 320;
        scr_h = 200;
        gfx_mode = 0;
    }
    stride = g_fb.pitch / 4;
    size_t bbsize = (size_t)scr_w * scr_h * 4;
    if (gfx_mode) {
        backbuf = (uint32_t *)mmap_user(0, bbsize, 3, 0);
        vesa_set_backbuffer(backbuf);
    }
    mouse_x = scr_w / 2;
    mouse_y = scr_h / 2;
    disp_create_window(30, 30, scr_w * 3 / 5, scr_h * 2 / 3 - PANEL_H, "Terminal");
}
