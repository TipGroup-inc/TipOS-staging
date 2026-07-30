/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: tui.h ~ funcoes anotadas: 0
 */
/* ~*~ tui.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef TUI_H
#define TUI_H

#define TUI_COLS 80
#define TUI_ROWS 25

/* ── Cores VGA (16 cores) ──────────────────────────── */
#define TUI_BLACK       0
#define TUI_BLUE        1
#define TUI_GREEN       2
#define TUI_CYAN        3
#define TUI_RED         4
#define TUI_MAGENTA     5
#define TUI_BROWN       6
#define TUI_LIGHT_GRAY  7
#define TUI_DARK_GRAY   8
#define TUI_LIGHT_BLUE  9
#define TUI_LIGHT_GREEN 10
#define TUI_LIGHT_CYAN  11
#define TUI_LIGHT_RED   12
#define TUI_LIGHT_MAGENTA 13
#define TUI_YELLOW      14
#define TUI_WHITE       15

/* ── Cores de sistema (match colors.h) ─────────────── */
#define TUI_C_PROMPT     TUI_LIGHT_GRAY
#define TUI_C_BG_PROMPT  TUI_BLACK
#define TUI_C_ERROR      TUI_LIGHT_RED
#define TUI_C_BG_ERROR   TUI_BLACK
#define TUI_C_SUCCESS    TUI_LIGHT_GREEN
#define TUI_C_BG_SUCCESS TUI_BLACK

/* ── Keys ──────────────────────────────────────────── */
#define TUI_KEY_ESC     27
#define TUI_KEY_BS      127
#define TUI_KEY_TAB     9
#define TUI_KEY_ENTER   10

#define TUI_KEY_UP      300
#define TUI_KEY_DOWN    301
#define TUI_KEY_LEFT    302
#define TUI_KEY_RIGHT   303
#define TUI_KEY_HOME    304
#define TUI_KEY_END     305
#define TUI_KEY_PGUP    306
#define TUI_KEY_PGDN    307
#define TUI_KEY_INS     308
#define TUI_KEY_DEL     309
#define TUI_KEY_F1      310
#define TUI_KEY_F2      311
#define TUI_KEY_F3      312
#define TUI_KEY_F4      313
#define TUI_KEY_F5      314
#define TUI_KEY_F6      315
#define TUI_KEY_F7      316
#define TUI_KEY_F8      317
#define TUI_KEY_F9      318
#define TUI_KEY_F10     319
#define TUI_KEY_F11     320
#define TUI_KEY_F12     321

/* ── Window flags ──────────────────────────────────── */
#define TUI_BORDER   1
#define TUI_SCROLL   4

/* ── Event types ───────────────────────────────────── */
#define TUI_EV_KEY   1
#define TUI_EV_IDLE  2

typedef struct {
    int type;
    int key;
} tui_event_t;

/* ── Opaque handle ─────────────────────────────────── */
typedef struct tui_s tui_t;
typedef struct tui_win_s tui_win_t;

/* ── Screen management ─────────────────────────────── */
tui_t *tui_init(void);
void   tui_end(tui_t *t);
void   tui_refresh(tui_t *t);

/* ── Window management ─────────────────────────────── */
tui_win_t *tui_win_new(tui_t *t, int x, int y, int w, int h, int flags);
void      tui_win_close(tui_t *t, tui_win_t *w);
void      tui_win_move(tui_win_t *w, int x, int y);
void      tui_win_resize(tui_win_t *w, int nw, int nh);
void      tui_win_raise(tui_t *t, tui_win_t *w);
void      tui_win_title(tui_win_t *w, const char *s);

/* ── Drawing ───────────────────────────────────────── */
void tui_win_gotoxy(tui_win_t *w, int y, int x);
void tui_win_addch(tui_win_t *w, char ch);
void tui_win_addch_attr(tui_win_t *w, char ch, int fg, int bg);
void tui_win_addstr(tui_win_t *w, const char *s);
void tui_win_printf(tui_win_t *w, const char *fmt, ...);
void tui_win_color(tui_win_t *w, int fg, int bg);
void tui_win_bold(tui_win_t *w, int on);
void tui_win_clear(tui_win_t *w);
void tui_win_clrtoeol(tui_win_t *w);
void tui_win_box(tui_win_t *w, const char *title);
void tui_win_fill(tui_win_t *w, char ch);

/* ── Scrolling ─────────────────────────────────────── */
void tui_win_scroll(tui_win_t *w, int lines);
void tui_win_scroll_ok(tui_win_t *w, int on);

/* ── Input ─────────────────────────────────────────── */
int tui_getch(tui_t *t);
int tui_poll_event(tui_t *t, tui_event_t *ev);

/* ── Utilities ─────────────────────────────────────── */
int tui_width(tui_t *t);
int tui_height(tui_t *t);
int tui_win_width(tui_win_t *w);
int tui_win_height(tui_win_t *w);

/* ── High-level widgets ────────────────────────────── */
int  tui_dialog(tui_t *t, const char *title, const char *msg, const char *buttons);
int  tui_prompt(tui_t *t, const char *label, char *out, int max);
void tui_msgbox(tui_t *t, const char *title, const char *msg);

#endif




/* ♥ tui.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
