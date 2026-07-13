#include <stdint.h>
#include "vesa.h"

#define MAX_WINDOWS 8
#define TITLE_H 16
#define CLOSE_W 14

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
    int dirty;
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count;
static int focused_win = -1;
static int mouse_x, mouse_y;
static int old_mx = -1, old_my = -1;

// ─── drag state ──────────────────────────────────────────
enum { DRAG_NONE, DRAG_MOVE };
static int drag_state = DRAG_NONE;
static int drag_win = -1;
static int drag_off_x, drag_off_y;

// ─── window API ──────────────────────────────────────────
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

static void raise_window(int idx) {
    if (idx < 0 || idx >= win_count || idx == win_count - 1) return;
    window_t tmp = windows[idx];
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    windows[win_count - 1] = tmp;
    focused_win = win_count - 1;
    for (int i = 0; i < win_count; i++) windows[i].dirty = 1;
    windows[focused_win].focused = 1;
}

static void close_window(int idx) {
    if (idx < 0 || idx >= win_count) return;
    for (int i = idx; i < win_count - 1; i++) windows[i] = windows[i + 1];
    win_count--;
    focused_win = win_count - 1;
    if (focused_win >= 0) windows[focused_win].focused = 1;
    old_mx = -1; // force full redraw next frame
}

// ─── hit testing ─────────────────────────────────────────
enum { HIT_NONE, HIT_TITLE, HIT_CLOSE, HIT_CLIENT };

static int hit_test(int mx, int my, int *out_idx) {
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

// ─── drag API ────────────────────────────────────────────
int disp_drag_state(void) { return drag_state; }
int disp_drag_win(void) { return drag_win; }
void disp_drag_move(int dx, int dy) {
    if (drag_win < 0 || drag_win >= win_count) return;
    window_t *w = &windows[drag_win];
    w->x += dx; w->y += dy;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->x + w->w > scr_w) w->x = scr_w - w->w;
    if (w->y + w->h > scr_h) w->y = scr_h - w->h;
    w->dirty = 1;
    old_mx = -1;
}

// ─── click handling ──────────────────────────────────────
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
}

void disp_close_focused(void) {
    if (focused_win >= 0) close_window(focused_win);
}

void disp_cycle_focus(void) {
    if (win_count <= 1) return;
    int idx = focused_win;
    if (idx < 0) idx = 0;
    int next = (idx + 1) % win_count;
    for (int i = 0; i < win_count; i++) windows[i].dirty = 1;
    windows[idx].focused = 0;
    windows[next].focused = 1;
    focused_win = next;
}

// ─── drawing ─────────────────────────────────────────────
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

// ─── cursor save/restore ─────────────────────────────────
static uint32_t cur_bg[12][12];

static void cur_save(int mx, int my) {
    extern framebuffer_t g_fb;
    uint32_t *fb = (uint32_t *)(uintptr_t)g_fb.addr;
    uint32_t stride = g_fb.pitch / 4;
    for (int y = 0; y < 12 && my + y < scr_h; y++)
        for (int x = 0; x < 12 && mx + x < scr_w; x++)
            cur_bg[y][x] = fb[(my + y) * stride + (mx + x)];
}

static void cur_restore(int mx, int my) {
    for (int y = 0; y < 12 && my + y < scr_h; y++)
        for (int x = 0; x < 12 && mx + x < scr_w; x++)
            vesa_draw_pixel(mx + x, my + y, cur_bg[y][x]);
}

static void cur_draw(int mx, int my) {
    uint32_t w = 0x00FFFFFF, b = 0x00000000;
    int cx = mx, cy = my;
    vesa_draw_pixel(cx,   cy,   w);
    vesa_draw_pixel(cx,   cy+1, w); vesa_draw_pixel(cx+1, cy+1, w);
    vesa_draw_pixel(cx,   cy+2, w); vesa_draw_pixel(cx+2, cy+2, w);
    vesa_draw_pixel(cx,   cy+3, w); vesa_draw_pixel(cx+3, cy+3, w);
    vesa_draw_pixel(cx,   cy+4, w); vesa_draw_pixel(cx+4, cy+4, b); vesa_draw_pixel(cx+5, cy+4, b);
    vesa_draw_pixel(cx,   cy+5, w); vesa_draw_pixel(cx+5, cy+5, w);
    vesa_draw_pixel(cx,   cy+6, w); vesa_draw_pixel(cx+6, cy+6, w);
    vesa_draw_pixel(cx,   cy+7, w); vesa_draw_pixel(cx+7, cy+7, w);
    vesa_draw_pixel(cx+1, cy+7, b); vesa_draw_pixel(cx+2, cy+7, b);
    vesa_draw_pixel(cx+1, cy+6, b);
    vesa_draw_pixel(cx+2, cy+6, w);
    vesa_draw_pixel(cx+3, cy+7, w);
    vesa_draw_pixel(cx+4, cy+8, w);
    vesa_draw_pixel(cx+5, cy+9, w);
    vesa_draw_pixel(cx+6, cy+10, w);
    vesa_draw_pixel(cx+7, cy+11, w);
    vesa_draw_pixel(cx+7, cy+10, b); vesa_draw_pixel(cx+6, cy+9, b);
    vesa_draw_pixel(cx+5, cy+8, b);
}

// ─── render ──────────────────────────────────────────────
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
    if (old_mx < 0) {
        for (int y = 0; y < scr_h; y++) {
            uint32_t c = 0x00001030 | ((y * 28 / scr_h) << 16) | ((y * 12 / scr_h) << 8);
            vesa_draw_rect(0, y, scr_w, 1, c);
        }
        for (int i = 0; i < win_count; i++) {
            draw_titlebar(&windows[i]);
            draw_client(&windows[i]);
        }
    } else {
        cur_restore(old_mx, old_my);
        for (int i = 0; i < win_count; i++) {
            window_t *w = &windows[i];
            if (!w->dirty) continue;
            draw_titlebar(w);
            draw_client(w);
            w->dirty = 0;
        }
    }
    cur_save(mouse_x, mouse_y);
    cur_draw(mouse_x, mouse_y);
    old_mx = mouse_x;
    old_my = mouse_y;
}

int disp_win_count(void) { return win_count; }

// ─── init ────────────────────────────────────────────────
void disp_init(void) {
    win_count = 0;
    focused_win = -1;
    drag_state = DRAG_NONE;
    drag_win = -1;
    old_mx = -1;
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
