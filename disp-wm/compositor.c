#include "libdisp.h"
#include <stdint.h>
#include <stddef.h>

// ─── Constants ─────────────────────────────────────────────────
#define MAX_WINDOWS 8
#define TITLE_H 16
#define CLOSE_W 14
#define PANEL_H 24
#define NEW_BTN_W 20

// ─── Framebuffer info (set by main) ─────────────────
static uint32_t *fb = NULL;
static uint32_t *backbuf = NULL;
static int scr_w = 1024;
static int scr_h = 768;
static uint32_t stride = 0;

// ─── Fast fill / copy helpers (inline asm rep) ────────────────
static inline void fill32(uint32_t *dst, uint32_t val, size_t count) {
    __asm__ volatile ("rep stosl" : "+D"(dst), "+c"(count) : "a"(val) : "memory");
}

static inline void copy32(uint32_t *dst, const uint32_t *src, size_t count) {
    __asm__ volatile ("rep movsl" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
}

// ─── Low-level drawing (writes to backbuf) ─────────────────────
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > scr_w) x1 = scr_w;
    int y1 = y + h; if (y1 > scr_h) y1 = scr_h;
    size_t row_w = (size_t)(x1 - x0);
    for (int py = y0; py < y1; py++)
        fill32(&backbuf[(size_t)py * stride + x0], color, row_w);
}

static void fill_screen(uint32_t color) {
    fill32(backbuf, color, (size_t)scr_w * scr_h);
}

static void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= scr_w || y >= scr_h) return;
    backbuf[y * stride + x] = color;
}

static void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) return;
    // Bitmap font 8x8 embedded below
    extern const uint8_t _font8x8[95][8];
    const uint8_t *glyph = _font8x8[c - 32];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row / 2];
        int py = y + row;
        for (int col = 0; col < 8; col++)
            if (bits & (1 << col))
                draw_pixel(x + col, py, color);
    }
}

// ─── Window struct ─────────────────────────────────────────────
typedef struct {
    int x, y, w, h;
    uint8_t focused;
    char title[32];
} window_t;

static window_t windows[MAX_WINDOWS];
static int win_count = 0;
static int focused_win = -1;

// ─── Mouse state ───────────────────────────────────────────────
static int mouse_x = 0, mouse_y = 0;
static int old_mx = -1, old_my = -1;

// ─── Drag state ────────────────────────────────────────────────
enum { DRAG_NONE, DRAG_MOVE };
static int drag_state = DRAG_NONE;
static int drag_win = -1;

static int next_cascade = 30;

// ─── Window management ─────────────────────────────────────────
static int create_window(int x, int y, int w, int h, const char *title) {
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

static int new_window(const char *title) {
    int w = scr_w * 3 / 5;
    int h = scr_h * 2 / 3 - PANEL_H;
    if (h < 100) h = 100;
    int x = next_cascade;
    int y = next_cascade;
    next_cascade = (next_cascade + 30) % (scr_w / 2);
    if (x + w > scr_w) x = 0;
    if (y + h > scr_h - PANEL_H) y = 0;
    return create_window(x, y, w, h, title);
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

// ─── Hit testing ───────────────────────────────────────────────
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

// ─── Click handling ────────────────────────────────────────────
static void handle_click(void) {
    if (drag_state == DRAG_MOVE) {
        drag_state = DRAG_NONE;
        drag_win = -1;
        return;
    }
    int wi;
    switch (hit_test(mouse_x, mouse_y, &wi)) {
        case HIT_CLOSE:
            close_window(wi);
            return;
        case HIT_NEW_BTN:
            new_window("Terminal");
            return;
        case HIT_TITLE:
            raise_window(wi);
            drag_state = DRAG_MOVE;
            drag_win = wi;
            return;
        case HIT_CLIENT:
            raise_window(wi);
            return;
        case HIT_PANEL: {
            int pwi;
            if (panel_btn_hit(mouse_x, &pwi))
                raise_window(pwi);
            return;
        }
        default: break;
    }
}

static void close_focused(void) {
    if (focused_win >= 0) close_window(focused_win);
}

static void cycle_focus(void) {
    if (win_count <= 1) return;
    int idx = focused_win;
    if (idx < 0) idx = 0;
    int next = (idx + 1) % win_count;
    windows[idx].focused = 0;
    windows[next].focused = 1;
    focused_win = next;
}

static void drag_move(int dx, int dy) {
    if (drag_win < 0 || drag_win >= win_count) return;
    window_t *w = &windows[drag_win];
    w->x += dx; w->y += dy;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->x + w->w > scr_w) w->x = scr_w - w->w;
    if (w->y + w->h > scr_h - PANEL_H) w->y = scr_h - PANEL_H - w->h;
    mouse_x += dx; mouse_y += dy;
}

// ─── Drawing ───────────────────────────────────────────────────
static void draw_titlebar(window_t *w) {
    uint32_t bg = w->focused ? 0x000044AA : 0x00223366;
    fill_rect(w->x, w->y, w->w - CLOSE_W, TITLE_H, bg);
    fill_rect(w->x + w->w - CLOSE_W, w->y, CLOSE_W, TITLE_H, 0x00AA2222);
    draw_char(w->x + w->w - 11, w->y + 2, 'x', 0x00FFFFFF);
    int i = 0;
    for (int tx = w->x + 4; tx < w->x + w->w - CLOSE_W - 4 && w->title[i]; tx += 8, i++)
        draw_char(tx, w->y + 2, w->title[i], 0x00FFFFFF);
    fill_rect(w->x, w->y + TITLE_H, w->w, 2, 0x006688CC);
}

static void draw_client(window_t *w) {
    fill_rect(w->x, w->y + TITLE_H + 2, w->w, w->h - TITLE_H - 2, 0x00EEEEEE);
}

static void draw_panel(void) {
    fill_rect(0, scr_h - PANEL_H, scr_w, PANEL_H, 0x001A1A2E);
    fill_rect(0, scr_h - PANEL_H, scr_w, 1, 0x00333366);
    int x = 4;
    for (int i = 0; i < win_count; i++) {
        window_t *w = &windows[i];
        int bw = 8;
        for (int j = 0; w->title[j]; j++) bw += 8;
        if (bw > 200) bw = 200;
        uint32_t btn = (i == focused_win) ? 0x00335599 : 0x00223355;
        fill_rect(x, scr_h - PANEL_H + 3, bw, PANEL_H - 6, btn);
        fill_rect(x, scr_h - PANEL_H + 3, bw, 1, 0x004466AA);
        int tx = x + 4;
        int ti = 0;
        for (; tx < x + bw - 4 && w->title[ti]; tx += 8, ti++)
            draw_char(tx, scr_h - PANEL_H + 6, w->title[ti], 0x00CCCCCC);
        x += bw + 4;
    }
    int bx = scr_w - NEW_BTN_W - 4;
    fill_rect(bx, scr_h - PANEL_H + 3, NEW_BTN_W, PANEL_H - 6, 0x00225533);
    draw_char(bx + 6, scr_h - PANEL_H + 5, '+', 0x00AAFFAA);
}

// ─── Cursor ────────────────────────────────────────────────────
static uint32_t cur_bg[8][8];

static void cur_save(int mx, int my) {
    for (int y = 0; y < 8 && my + y < scr_h; y++)
        copy32(cur_bg[y], &backbuf[(size_t)(my + y) * stride + mx], 8);
}

static void cur_restore(int mx, int my) {
    for (int y = 0; y < 8 && my + y < scr_h; y++)
        copy32(&backbuf[(size_t)(my + y) * stride + mx], cur_bg[y], 8);
}

static void cur_draw(int mx, int my) {
    fill_rect(mx, my, 8, 8, 0x00FFFFFF);
    fill_rect(mx + 2, my + 2, 4, 4, 0x00000000);
}

// ─── Render ────────────────────────────────────────────────────
static void render(void) {
    if (old_mx >= 0) cur_restore(old_mx, old_my);
    fill_screen(0x00001030);
    for (int i = 0; i < win_count; i++) {
        draw_titlebar(&windows[i]);
        draw_client(&windows[i]);
    }
    draw_panel();
    cur_save(mouse_x, mouse_y);
    cur_draw(mouse_x, mouse_y);
    libdisp_flush(backbuf);
    old_mx = mouse_x;
    old_my = mouse_y;
}

// ─── Syscall wrappers (from libc / inline) ────────────────────
static void write_str(const char *s) {
    while (*s) {
        __asm__ volatile ("int $0x80" : : "a"(4ULL), "b"(1ULL), "c"(s), "d"(1ULL) : "memory");
        s++;
    }
}

static void kbd_read(char *buf, int count) {
    __asm__ volatile ("int $0x80" : "=a"(*(uint64_t*)buf) : "a"(3ULL), "b"(0ULL), "c"(buf), "d"((uint64_t)count) : "memory");
}

// ─── Entry point (CRT0 chama main()) ─────────────────────────
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t fb_addr;
    uint32_t w, h, pitch;

    if (libdisp_init(&fb_addr, &w, &h, &pitch) < 0) {
        write_str("disp-wm: no framebuffer available\n");
        return 1;
    }

    fb = (uint32_t *)(uintptr_t)fb_addr;
    scr_w = (int)w;
    scr_h = (int)h;
    stride = pitch / 4;

    // Allocate backbuffer via mmap
    size_t bb_size = (size_t)scr_w * scr_h * 4;
    uint64_t map_addr = 0;
    __asm__ volatile ("int $0x80"
        : "=a"(map_addr)
        : "a"(197ULL), "b"(0ULL), "c"(bb_size), "d"(3ULL), "S"(0x1002ULL)
        : "memory");
    backbuf = (uint32_t *)(uintptr_t)map_addr;
    if (!backbuf || backbuf == (uint32_t*)-1) {
        write_str("disp-wm: backbuffer alloc failed\n");
        return 1;
    }

    mouse_x = scr_w / 2;
    mouse_y = scr_h / 2;
    create_window(30, 30, scr_w * 3 / 5, scr_h * 2 / 3 - PANEL_H, "Terminal");
    render();

    while (1) {
        char ch = 0;
        kbd_read(&ch, 1);
        int step = 5;

        switch (ch) {
            case 27: goto exit;
            case ' ': handle_click(); break;
            case '\t': cycle_focus(); break;
            case 0x0E: new_window("Terminal"); break;  // Ctrl+N
            case 0x11: close_focused(); break;          // Ctrl+Q
            case 'w': case 'W':
                if (drag_state) drag_move(0, -step);
                else { if (mouse_y > 0) mouse_y -= step; }
                break;
            case 's': case 'S':
                if (drag_state) drag_move(0, step);
                else { if (mouse_y < scr_h - 1) mouse_y += step; }
                break;
            case 'a': case 'A':
                if (drag_state) drag_move(-step, 0);
                else { if (mouse_x > 0) mouse_x -= step; }
                break;
            case 'd': case 'D':
                if (drag_state) drag_move(step, 0);
                else { if (mouse_x < scr_w - 1) mouse_x += step; }
                break;
        }

        render();
        if (win_count == 0) goto exit;
    }

exit:
    libdisp_flush(backbuf);
    return 0;  // main retorna → CRT0 chama exit(0)
}
