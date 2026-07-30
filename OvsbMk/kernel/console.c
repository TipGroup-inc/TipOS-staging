/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: console.c ~ funcoes anotadas: 11
 */
/* ♥ console.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include "console.h"
#include <stddef.h>
#include "serial.h"
#include "../lib/gui/vesa.h"

#define CONSOLE_COLS 80
#define CONSOLE_ROWS 45
#define SCROLLBACK  200

static char scrollback[SCROLLBACK][CONSOLE_COLS];
static int  cursor_row = 0, cursor_col = 0;
static int  scroll_start = 0;
static uint32_t fg_color = 0xFFC0C0C0;
static uint32_t bg_color = 0xFF0A0A1A;
static int cursor_visible = 1;

/* Output redirection for terminal */
static char *term_out_buf = NULL;
static int  *term_out_len = NULL;
static int   term_out_max = 0;

static int scr_w, scr_h;
static int col_w = 8, row_h = 16;

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void console_init(void) {
    extern framebuffer_t g_fb;
    scr_w = g_fb.width;
    scr_h = g_fb.height;
    for (int i = 0; i < SCROLLBACK; i++)
        for (int j = 0; j < CONSOLE_COLS; j++)
            scrollback[i][j] = ' ';
    cursor_row = 0; cursor_col = 0; scroll_start = 0;
    vesa_fill_screen(bg_color);
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
void console_set_fg(uint32_t color) { fg_color = color; }
/* ~~ Setando~~ espero que saiba o que ta fazendo! */
void console_set_bg(uint32_t color) { bg_color = color; }

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void newline(void) {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= CONSOLE_ROWS) {
        cursor_row = CONSOLE_ROWS - 1;
        for (int i = 0; i < SCROLLBACK - 1; i++)
            for (int j = 0; j < CONSOLE_COLS; j++)
                scrollback[i][j] = scrollback[i + 1][j];
        for (int j = 0; j < CONSOLE_COLS; j++)
            scrollback[SCROLLBACK - 1][j] = ' ';
        scroll_start = 0;
    }
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int abs_row(int r) {
    return scroll_start + r;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void render_row(int r) {
    int ar = abs_row(r);
    if (ar < 0 || ar >= SCROLLBACK) return;
    int y = r * row_h;
    for (int c = 0; c < CONSOLE_COLS; c++)
        vesa_draw_cell(c * col_w, y, scrollback[ar][c], fg_color, bg_color);
}

/* ~ cuidado que essa aqui morde ~ */
static void render_all(void) {
    for (int r = 0; r < CONSOLE_ROWS; r++)
        render_row(r);
}

/* ~~ console_clear ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void console_clear(void) {
    for (int i = 0; i < SCROLLBACK; i++)
        for (int j = 0; j < CONSOLE_COLS; j++)
            scrollback[i][j] = ' ';
    cursor_row = 0; cursor_col = 0; scroll_start = 0;
    vesa_fill_screen(bg_color);
}

/* ~~ console_putchar ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void console_putchar(char c) {
    if (term_out_buf && term_out_len) {
        if (*term_out_len < term_out_max - 1) {
            term_out_buf[(*term_out_len)++] = c;
            term_out_buf[*term_out_len] = '\0';
        }
        serial_putc(c);
        return;
    }
    if (c == '\n') {
        int ar = abs_row(cursor_row);
        scrollback[ar][cursor_col] = ' ';
        newline();
        render_all();
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            int ar = abs_row(cursor_row);
            scrollback[ar][cursor_col] = ' ';
            render_row(cursor_row);
        }
    } else if (c >= ' ') {
        int ar = abs_row(cursor_row);
        if (cursor_col >= CONSOLE_COLS) newline();
        ar = abs_row(cursor_row);
        scrollback[ar][cursor_col] = c;
        render_row(cursor_row);
        cursor_col++;
    }
    serial_putc(c);
}

/* ~~ Escrevendo~~ não vai corromper nada, vai? >_< */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void console_write(const char *s) {
    while (*s) { console_putchar(*s); s++; }
}

/* ~~ output redirect hook for terminal ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void console_set_output_buffer(char *buf, int *len, int max) {
    term_out_buf = buf;
    term_out_len = len;
    term_out_max = max;
}

/* ~~ console_show_cursor ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void console_show_cursor(int show) {
    cursor_visible = show;
}

#include <stdarg.h>
/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void console_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { buf[n++] = *p; continue; }
        p++;
        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                while (*s) buf[n++] = *s++;
                break;
            }
            case 'd': {
                int v = va_arg(args, int);
                char tmp[16]; int ti = 0;
                if (v < 0) { buf[n++] = '-'; v = -v; }
                if (v == 0) tmp[ti++] = '0';
                while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
                while (ti > 0) buf[n++] = tmp[--ti];
                break;
            }
            case 'x': case 'X': {
                unsigned int v = va_arg(args, unsigned int);
                char tmp[16]; int ti = 0;
                if (v == 0) tmp[ti++] = '0';
                while (v > 0) {
                    int d = v % 16;
                    tmp[ti++] = d < 10 ? '0' + d : 'a' + d - 10;
                    v /= 16;
                }
                buf[n++] = '0'; buf[n++] = 'x';
                while (ti > 0) buf[n++] = tmp[--ti];
                break;
            }
        }
        if (n >= 250) break;
    }
    buf[n] = 0;
    va_end(args);
    console_write(buf);
}

/* ♥ console.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */




/* ♥ console.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
