/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: tui.c ~ funcoes anotadas: 37
 */
/* ~*~ tui.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include <tui.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/* ── Internal helpers ──────────────────────────────────── */

/* Write raw bytes to stdout (bypasses printf's per-char overhead) */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void out(const char *s, int n) {
    write(1, s, n);
}
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void outs(const char *s) {
    write(1, s, strlen(s));
}
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void outc(char c) {
    write(1, &c, 1);
}

/* Format integer into buf, return length */
/* ~ essa demorou pra debugar, respeita ~ */
static int fmt_int(char *buf, int n) {
    int p = 0;
    if (n < 0) { buf[p++] = '-'; n = -n; }
    char tmp[12], *t = tmp;
    do { *t++ = '0' + (n % 10); n /= 10; } while (n);
    while (t > tmp) buf[p++] = *--t;
    return p;
}

/* ANSI cursor goto: \x1b[row;colH */
/* ~ essa demorou pra debugar, respeita ~ */
static void ansi_goto(int y, int x) {
    char buf[24]; int p = 0;
    buf[p++] = 0x1B; buf[p++] = '[';
    p += fmt_int(buf + p, y + 1);
    buf[p++] = ';';
    p += fmt_int(buf + p, x + 1);
    buf[p++] = 'H';
    out(buf, p);
}

/* ANSI set attribute: \x1b[ATTRm */
/* ~ cuidado que essa aqui morde ~ */
static void ansi_attr(int fg, int bg, int bold) {
    char buf[32]; int p = 0;
    buf[p++] = 0x1B; buf[p++] = '[';
    if (bold) { buf[p++] = '1'; buf[p++] = ';'; }
    /* VGA 0-15 → ANSI 30-37 / 90-97 */
    int ansi_fg = (fg & 8) ? 90 + (fg & 7) : 30 + fg;
    int ansi_bg = (bg & 8) ? 100 + (bg & 7) : 40 + bg;
    p += fmt_int(buf + p, ansi_fg);
    buf[p++] = ';';
    p += fmt_int(buf + p, ansi_bg);
    buf[p++] = 'm';
    out(buf, p);
}

/* ── Cell ──────────────────────────────────────────────── */
typedef struct {
    char ch;
    unsigned char attr;   /* VGA attribute byte: bg<<4 | fg */
} cell_t;

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static unsigned char make_attr(int fg, int bg) {
    return (unsigned char)((bg << 4) | fg);
}

/* ── Window ────────────────────────────────────────────── */
struct tui_win_s {
    int x, y, w, h;
    int flags;
    int cx, cy;          /* cursor within window (relative) */
    int scroll_off;      /* lines scrolled up */
    int scroll_ok;
    int fg, bg, bold;
    cell_t *buf;         /* h × w cells */
    int *dirty;          /* per-line: 1 = line changed */
    char *title;
};

static tui_win_t *win_new(tui_t *t, int x, int y, int w, int h, int flags);

/* ── TUI context ───────────────────────────────────────── */
struct tui_s {
    int w, h;                    /* screen dimensions */
    tui_win_t **windows;
    int nwindows;
    int cap;
    cell_t *composite;           /* what should be on screen */
    int *comp_dirty;             /* per-line composite dirty */
    cell_t *phys;                /* what's currently on terminal */
};

/* ── Screen init / end ────────────────────────────────── */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
tui_t *tui_init(void) {
    tui_t *t = malloc(sizeof(tui_t));
    if (!t) return 0;
    t->w = TUI_COLS;
    t->h = TUI_ROWS;
    t->windows = 0;
    t->nwindows = 0;
    t->cap = 0;
    t->composite = malloc(t->w * t->h * sizeof(cell_t));
    t->phys = malloc(t->w * t->h * sizeof(cell_t));
    t->comp_dirty = malloc(t->h * sizeof(int));
    if (!t->composite || !t->phys || !t->comp_dirty) {
        free(t->composite); free(t->phys); free(t->comp_dirty); free(t);
        return 0;
    }
    /* Init composite blank */
    cell_t blank = { ' ', make_attr(TUI_WHITE, TUI_BLACK) };
    for (int i = 0; i < t->w * t->h; i++) t->composite[i] = blank;
    for (int i = 0; i < t->w * t->h; i++) t->phys[i] = blank;
    for (int i = 0; i < t->h; i++) t->comp_dirty[i] = 1;
    /* Hide cursor, clear screen */
    outs("\x1b[?25l\x1b[2J");
    t->phys[0].ch = 0; /* force full redraw on first refresh */
    return t;
}

/* ~~ tui_end ~~ */
/* ~ cuidado que essa aqui morde ~ */
void tui_end(tui_t *t) {
    if (!t) return;
    for (int i = 0; i < t->nwindows; i++) {
        free(t->windows[i]->buf);
        free(t->windows[i]->dirty);
        free(t->windows[i]->title);
        free(t->windows[i]);
    }
    free(t->windows);
    free(t->composite);
    free(t->phys);
    free(t->comp_dirty);
    outs("\x1b[2J\x1b[H\x1b[?25h");
    free(t);
}

/* ── Window management ────────────────────────────────── */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
tui_win_t *tui_win_new(tui_t *t, int x, int y, int w, int h, int flags) {
    if (!t || w < 1 || h < 1) return 0;
    tui_win_t *win = malloc(sizeof(tui_win_t));
    if (!win) return 0;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->flags = flags;
    win->cx = 0; win->cy = 0;
    win->scroll_off = 0;
    win->scroll_ok = (flags & TUI_SCROLL) ? 1 : 0;
    win->fg = TUI_WHITE; win->bg = TUI_BLACK; win->bold = 0;
    win->buf = malloc(w * h * sizeof(cell_t));
    win->dirty = malloc(h * sizeof(int));
    win->title = 0;
    if (!win->buf || !win->dirty) {
        free(win->buf); free(win->dirty); free(win);
        return 0;
    }
    cell_t blank = { ' ', make_attr(win->fg, win->bg) };
    for (int i = 0; i < w * h; i++) win->buf[i] = blank;
    for (int i = 0; i < h; i++) win->dirty[i] = 1;

    if (t->nwindows >= t->cap) {
        int nc = t->cap ? t->cap * 2 : 8;
        tui_win_t **nw = realloc(t->windows, nc * sizeof(tui_win_t*));
        if (!nw) { free(win->buf); free(win->dirty); free(win); return 0; }
        t->windows = nw;
        t->cap = nc;
    }
    t->windows[t->nwindows++] = win;
    return win;
}

/* ~~ tui_win_close ~~ */
/* ~ cuidado que essa aqui morde ~ */
void tui_win_close(tui_t *t, tui_win_t *w) {
    if (!t || !w) return;
    for (int i = 0; i < t->nwindows; i++) {
        if (t->windows[i] == w) {
            free(w->buf); free(w->dirty); free(w->title); free(w);
            t->windows[i] = t->windows[--t->nwindows];
            /* Mark affected region dirty */
            for (int r = w->y; r < w->y + w->h && r < t->h; r++)
                if (r >= 0) t->comp_dirty[r] = 1;
            return;
        }
    }
}

/* ~~ Movendo~~ vai pra lá~ agora volta~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void tui_win_move(tui_win_t *w, int x, int y) {
    if (!w) return;
    w->x = x; w->y = y;
    for (int i = 0; i < w->h; i++) w->dirty[i] = 1;
}

/* ~~ tui_win_resize ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void tui_win_resize(tui_win_t *w, int nw, int nh) {
    if (!w || nw < 1 || nh < 1) return;
    cell_t *nb = malloc(nw * nh * sizeof(cell_t));
    int *nd = malloc(nh * sizeof(int));
    if (!nb || !nd) { free(nb); free(nd); return; }
    cell_t blank = { ' ', make_attr(w->fg, w->bg) };
    for (int i = 0; i < nw * nh; i++) nb[i] = blank;
    for (int i = 0; i < nh; i++) nd[i] = 1;
    free(w->buf); free(w->dirty);
    w->buf = nb; w->dirty = nd;
    w->w = nw; w->h = nh; w->cx = 0; w->cy = 0; w->scroll_off = 0;
}

/* ~~ tui_win_raise ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void tui_win_raise(tui_t *t, tui_win_t *w) {
    if (!t || !w) return;
    for (int i = 0; i < t->nwindows; i++) {
        if (t->windows[i] == w) {
            for (int j = i; j < t->nwindows - 1; j++)
                t->windows[j] = t->windows[j + 1];
            t->windows[t->nwindows - 1] = w;
            for (int r = w->y; r < w->y + w->h && r < t->h; r++)
                if (r >= 0) t->comp_dirty[r] = 1;
            return;
        }
    }
}

/* ~~ tui_win_title ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void tui_win_title(tui_win_t *w, const char *s) {
    if (!w) return;
    free(w->title);
    w->title = s ? strdup(s) : 0;
    for (int i = 0; i < w->h; i++) w->dirty[i] = 1;
}

/* ── Drawing helpers ──────────────────────────────────── */
/* ~ cuidado que essa aqui morde ~ */
static void mark_line(tui_win_t *w, int y) {
    if (y >= 0 && y < w->h) w->dirty[y] = 1;
}

/* ~~ tui_win_gotoxy ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void tui_win_gotoxy(tui_win_t *w, int y, int x) {
    if (!w) return;
    if (x < 0) x = 0; if (x >= w->w) x = w->w - 1;
    if (y < 0) y = 0; if (y >= w->h) y = w->h - 1;
    w->cx = x; w->cy = y;
}

/* ~~ tui_win_addch ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void tui_win_addch(tui_win_t *w, char ch) {
    if (!w || w->cx >= w->w || w->cy >= w->h) return;
    int idx = w->cy * w->w + w->cx;
    w->buf[idx].ch = ch;
    w->buf[idx].attr = make_attr(w->fg, w->bg);
    mark_line(w, w->cy);
    w->cx++;
}

/* ~~ tui_win_addch_attr ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void tui_win_addch_attr(tui_win_t *w, char ch, int fg, int bg) {
    if (!w || w->cx >= w->w || w->cy >= w->h) return;
    int idx = w->cy * w->w + w->cx;
    w->buf[idx].ch = ch;
    w->buf[idx].attr = make_attr(fg, bg);
    mark_line(w, w->cy);
    w->cx++;
}

/* ~~ tui_win_addstr ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void tui_win_addstr(tui_win_t *w, const char *s) {
    if (!w || !s) return;
    while (*s) {
        if (*s == '\n') {
            w->cx = 0; w->cy++;
            if (w->cy >= w->h && w->scroll_ok) {
                tui_win_scroll(w, 1);
                w->cy = w->h - 1;
            }
            s++;
            continue;
        }
        if (w->cx >= w->w) { w->cx = 0; w->cy++; }
        if (w->cy >= w->h) {
            if (w->scroll_ok) { tui_win_scroll(w, 1); w->cy = w->h - 1; }
            else break;
        }
        tui_win_addch(w, *s++);
    }
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void tui_win_vprintf(tui_win_t *w, const char *fmt, va_list ap) {
    char buf[256];
    int n = vsnprintf(buf, 256, fmt, ap);
    if (n > 0) tui_win_addstr(w, buf);
}

/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void tui_win_printf(tui_win_t *w, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    tui_win_vprintf(w, fmt, ap);
    va_end(ap);
}

/* ~~ tui_win_color ~~ */
/* ~ cuidado que essa aqui morde ~ */
void tui_win_color(tui_win_t *w, int fg, int bg) {
    if (!w) return;
    w->fg = fg & 15; w->bg = bg & 15;
}

/* ~~ tui_win_bold ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void tui_win_bold(tui_win_t *w, int on) {
    if (!w) return;
    w->bold = on;
}

/* ~~ tui_win_clear ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void tui_win_clear(tui_win_t *w) {
    if (!w) return;
    cell_t blank = { ' ', make_attr(w->fg, w->bg) };
    for (int i = 0; i < w->w * w->h; i++) w->buf[i] = blank;
    for (int i = 0; i < w->h; i++) w->dirty[i] = 1;
    w->cx = 0; w->cy = 0; w->scroll_off = 0;
}

/* ~~ tui_win_clrtoeol ~~ */
/* ~ cuidado que essa aqui morde ~ */
void tui_win_clrtoeol(tui_win_t *w) {
    if (!w || w->cy >= w->h) return;
    cell_t blank = { ' ', make_attr(w->fg, w->bg) };
    for (int x = w->cx; x < w->w; x++)
        w->buf[w->cy * w->w + x] = blank;
    mark_line(w, w->cy);
}

/* ~~ tui_win_box ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void tui_win_box(tui_win_t *w, const char *title) {
    if (!w || w->w < 3 || w->h < 3) return;
    cell_t blank = { ' ', make_attr(w->fg, w->bg) };
    /* top */
    w->buf[0 * w->w + 0].ch = '+';
    for (int x = 1; x < w->w - 1; x++) w->buf[0 * w->w + x].ch = '-';
    w->buf[0 * w->w + w->w - 1].ch = '+';
    /* bottom */
    w->buf[(w->h - 1) * w->w + 0].ch = '+';
    for (int x = 1; x < w->w - 1; x++) w->buf[(w->h - 1) * w->w + x].ch = '-';
    w->buf[(w->h - 1) * w->w + w->w - 1].ch = '+';
    /* sides */
    for (int y = 1; y < w->h - 1; y++) {
        w->buf[y * w->w + 0].ch = '|';
        w->buf[y * w->w + w->w - 1].ch = '|';
    }
    for (int y = 1; y < w->h - 1; y++)
        for (int x = 1; x < w->w - 1; x++)
            w->buf[y * w->w + x] = blank;
    /* title */
    if (title && *title) {
        int sl = strlen(title);
        int max = w->w - 3;
        if (sl > max) sl = max;
        for (int i = 0; i < sl; i++)
            w->buf[0 * w->w + 2 + i].ch = title[i];
    }
    for (int i = 0; i < w->h; i++) mark_line(w, i);
}

/* ~~ tui_win_fill ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void tui_win_fill(tui_win_t *w, char ch) {
    if (!w) return;
    for (int i = 0; i < w->w * w->h; i++) {
        w->buf[i].ch = ch;
        w->buf[i].attr = make_attr(w->fg, w->bg);
    }
    for (int i = 0; i < w->h; i++) w->dirty[i] = 1;
}

/* ── Scrolling ────────────────────────────────────────── */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void tui_win_scroll(tui_win_t *w, int lines) {
    if (!w || lines <= 0) return;
    int actual = lines < w->h ? lines : w->h;
    /* Shift lines up */
    for (int y = actual; y < w->h; y++)
        for (int x = 0; x < w->w; x++)
            w->buf[(y - actual) * w->w + x] = w->buf[y * w->w + x];
    /* Clear bottom lines */
    cell_t blank = { ' ', make_attr(w->fg, w->bg) };
    for (int y = w->h - actual; y < w->h; y++)
        for (int x = 0; x < w->w; x++)
            w->buf[y * w->w + x] = blank;
    for (int i = 0; i < w->h; i++) w->dirty[i] = 1;
    if (w->scroll_ok) w->scroll_off += actual;
}

/* ~~ tui_win_scroll_ok ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void tui_win_scroll_ok(tui_win_t *w, int on) {
    if (w) w->scroll_ok = on;
}

/* ── Compositing & Refresh ────────────────────────────── */

/* Composite: draw windows bottom to top into composite buffer */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void composite(tui_t *t) {
    /* Clear composite */
    cell_t blank = { ' ', make_attr(TUI_WHITE, TUI_BLACK) };
    for (int i = 0; i < t->w * t->h; i++) t->composite[i] = blank;

    /* Draw each window */
    for (int wi = 0; wi < t->nwindows; wi++) {
        tui_win_t *w = t->windows[wi];
        for (int row = 0; row < w->h; row++) {
            int sy = w->y + row;
            if (sy < 0 || sy >= t->h) continue;
            for (int col = 0; col < w->w; col++) {
                int sx = w->x + col;
                if (sx < 0 || sx >= t->w) continue;
                cell_t c = w->buf[row * w->w + col];
                t->composite[sy * t->w + sx] = c;
            }
        }
    }
}

/* ~~ tui_refresh ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void tui_refresh(tui_t *t) {
    if (!t) return;
    composite(t);

    for (int row = 0; row < t->h; row++) {
        cell_t *comp_row = &t->composite[row * t->w];
        cell_t *phys_row = &t->phys[row * t->w];
        int changed = 0;

        /* Check if row changed */
        for (int col = 0; col < t->w; col++) {
            if (comp_row[col].ch != phys_row[col].ch ||
                comp_row[col].attr != phys_row[col].attr) {
                changed = 1;
                break;
            }
        }
        if (!changed) continue;

        /* Send ANSI goto + attribute + row content */
        ansi_goto(row, 0);

        /* Emit attribute once for the row, then scan for changes */
        int cur_fg = -1, cur_bg = -1, cur_bold = -1;
        int last_nonblank = -1;

        /* Find last non-blank char (trim trailing spaces) */
        for (int col = t->w - 1; col >= 0; col--)
            if (comp_row[col].ch != ' ') { last_nonblank = col; break; }
        if (last_nonblank < 0) {
            /* Entire row is spaces — just clear to EOL */
            outs("\x1b[K");
            memcpy(phys_row, comp_row, t->w * sizeof(cell_t));
            continue;
        }

        for (int col = 0; col <= last_nonblank; col++) {
            int fg = comp_row[col].attr & 0x0F;
            int bg = (comp_row[col].attr >> 4) & 0x0F;
            /* Bold if fg has bit 8 (actually VGA attr only has 4-bit fg);
               we track bold separately via win->bold */
            int bold = 0; /* simplified: bold tracked per-window */

            if (fg != cur_fg || bg != cur_bg || bold != cur_bold) {
                cur_fg = fg; cur_bg = bg; cur_bold = bold;
                ansi_attr(fg, bg, bold);
            }

            char ch = comp_row[col].ch;
            if (ch < 0x20 && ch != '\t') ch = ' ';
            write(1, &ch, 1);
        }

        /* Clear rest of line if shorter than before */
        if (last_nonblank < t->w - 1) outs("\x1b[K");

        memcpy(phys_row, comp_row, t->w * sizeof(cell_t));
    }
}

/* ── Input ────────────────────────────────────────────── */

/* ~ essa demorou pra debugar, respeita ~ */
static int tui_parse_escape(tui_t *t) {
    char c = getchar();
    if (c != '[' && c != 'O') return TUI_KEY_ESC;
    char seq[8]; int n = 0;
    seq[n++] = 0x1B; seq[n++] = c;
    while (n < 7) {
        if (kbhit()) {
            seq[n++] = getchar();
            if (seq[n-1] >= 0x40 && seq[n-1] <= 0x7e) break;
            if (seq[n-1] == '~') break;
        } else {
            /* timeout — just ESC */
            return TUI_KEY_ESC;
        }
    }
    if (n == 3 && seq[1] == '[') {
        if (seq[2] >= 'A' && seq[2] <= 'D')
            return TUI_KEY_UP + (seq[2] - 'A');
        if (seq[2] == 'H') return TUI_KEY_HOME;
        if (seq[2] == 'F') return TUI_KEY_END;
    }
    if (n == 3 && seq[1] == 'O') {
        if (seq[2] >= 'A' && seq[2] <= 'D')
            return TUI_KEY_UP + (seq[2] - 'A');
        if (seq[2] == 'H') return TUI_KEY_HOME;
        if (seq[2] == 'F') return TUI_KEY_END;
        if (seq[2] >= 'P' && seq[2] <= 'S')
            return TUI_KEY_F1 + (seq[2] - 'P');
    }
    if (n >= 4 && seq[1] == '[') {
        if (seq[2] == '2' && seq[3] == '~') return TUI_KEY_INS;
        if (seq[2] == '3' && seq[3] == '~') return TUI_KEY_DEL;
        if (seq[2] == '5' && seq[3] == '~') return TUI_KEY_PGUP;
        if (seq[2] == '6' && seq[3] == '~') return TUI_KEY_PGDN;
        if (n >= 5) {
            if (seq[2] == '1' && seq[3] == '5' && seq[4] == '~') return TUI_KEY_F5;
            if (seq[2] == '1' && seq[3] == '7' && seq[4] == '~') return TUI_KEY_F6;
            if (seq[2] == '1' && seq[3] == '8' && seq[4] == '~') return TUI_KEY_F7;
            if (seq[2] == '1' && seq[3] == '9' && seq[4] == '~') return TUI_KEY_F8;
            if (seq[2] == '2' && seq[3] == '0' && seq[4] == '~') return TUI_KEY_F9;
            if (seq[2] == '2' && seq[3] == '1' && seq[4] == '~') return TUI_KEY_F10;
            if (seq[2] == '2' && seq[3] == '3' && seq[4] == '~') return TUI_KEY_F11;
            if (seq[2] == '2' && seq[3] == '4' && seq[4] == '~') return TUI_KEY_F12;
        }
    }
    return TUI_KEY_ESC;
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int tui_getch(tui_t *t) {
    (void)t;
    char c = getchar();
    if (c != 0x1B) return (unsigned char)c;
    return tui_parse_escape(t);
}

/* ~~ tui_poll_event ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int tui_poll_event(tui_t *t, tui_event_t *ev) {
    if (!t || !ev) return 0;
    if (!kbhit()) return 0;
    ev->type = TUI_EV_KEY;
    ev->key = tui_getch(t);
    return 1;
}

/* ── Dimensions ───────────────────────────────────────── */
int tui_width(tui_t *t) { return t ? t->w : 0; }
/* ~~ tui_height ~~ */
int tui_height(tui_t *t) { return t ? t->h : 0; }
/* ~~ tui_win_width ~~ */
int tui_win_width(tui_win_t *w) { return w ? w->w : 0; }
/* ~~ tui_win_height ~~ */
int tui_win_height(tui_win_t *w) { return w ? w->h : 0; }

/* ── Widgets ──────────────────────────────────────────── */

/* ~~ tui_dialog ~~ */
int tui_dialog(tui_t *t, const char *title, const char *msg,
               const char *buttons) {
    if (!t || !msg) return -1;
    int bw = t->w * 2 / 3; if (bw < 30) bw = 30; if (bw > t->w) bw = t->w;
    int bh = 7;
    int bx = (t->w - bw) / 2;
    int by = (t->h - bh) / 2;

    tui_win_t *w = tui_win_new(t, bx, by, bw, bh, 0);
    if (!w) return -1;
    tui_win_box(w, title ? title : "");
    tui_win_color(w, TUI_WHITE, TUI_BLACK);

    /* Message */
    int my = 1;
    const char *p = msg;
    while (*p) {
        int col = 0;
        while (*p && *p != '\n' && col < bw - 2) {
            tui_win_gotoxy(w, my, 2 + col);
            tui_win_addch(w, *p++);
            col++;
        }
        if (*p == '\n') p++;
        my++;
        if (my >= bh - 1) break;
    }

    /* Buttons */
    int bkey = -1;
    if (buttons && *buttons) {
        int bn = 0;
        while (buttons[bn]) bn++;
        int blen = bn + 2; /* [button] */
        int bstart = (bw - blen) / 2;
        if (bstart < 0) bstart = 0;
        tui_win_gotoxy(w, bh - 1, bstart);
        tui_win_addch(w, '[');
        tui_win_addstr(w, buttons);
        tui_win_addch(w, ']');
    }

    tui_refresh(t);

    /* Wait for key */
    int key = tui_getch(t);
    if (key == TUI_KEY_ENTER && buttons) bkey = 0;
    else if (key == TUI_KEY_ESC) bkey = -1;

    tui_win_close(t, w);
    tui_refresh(t);
    return bkey;
}

/* ~~ tui_msgbox ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void tui_msgbox(tui_t *t, const char *title, const char *msg) {
    tui_dialog(t, title, msg, "OK");
}

/* ~~ tui_prompt ~~ */
/* ~ cuidado que essa aqui morde ~ */
int tui_prompt(tui_t *t, const char *label, char *out, int max) {
    if (!t || !out || max < 1) return -1;
    int pw = t->w * 2 / 3; if (pw < 30) pw = 30; if (pw > t->w) pw = t->w;
    int ph = 5;
    int px = (t->w - pw) / 2;
    int py = (t->h - ph) / 2;

    tui_win_t *w = tui_win_new(t, px, py, pw, ph, 0);
    if (!w) return -1;
    tui_win_box(w, label ? label : "");
    tui_win_color(w, TUI_WHITE, TUI_BLACK);
    tui_refresh(t);

    int pos = 0;
    memset(out, 0, max);
    while (1) {
        tui_win_clear(w);
        tui_win_box(w, label ? label : "");
        tui_win_gotoxy(w, 2, 2);
        tui_win_addstr(w, out);
        tui_refresh(t);

        int k = tui_getch(t);
        if (k == TUI_KEY_ENTER) break;
        if (k == TUI_KEY_ESC) { pos = 0; out[0] = 0; break; }
        if ((k == TUI_KEY_BS || k == 127) && pos > 0) {
            out[--pos] = 0;
        }
        if (k >= 32 && k <= 126 && pos < max - 1) {
            out[pos++] = k; out[pos] = 0;
        }
    }
    tui_win_close(t, w);
    tui_refresh(t);
    return pos;
}




/* ♥ tui.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
