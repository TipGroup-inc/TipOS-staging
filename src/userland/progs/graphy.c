/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: graphy.c ~ funcoes anotadas: 23
 */
 /*~*~ graphy.c ~*~
  * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
  * Escrito com muito amor (e gambiarras) pela equipe TipOS!
  * Se quebrar, a culpa é sua~ <3
  *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

// ~~ graphy.c ~~
// Editor de texto estilo "nano" rodando sobre TUI.
// Operações: inserir, deletar, undo, cut/paste, find/replace,
// syntax highlight pra C, line numbers, scroll, busca bracket matching.
//
// Estrutura de dados: buffer linear com array de line starts (lns[]).
// O buffer é um char* dinâmico (realloc), as linhas são índices nele.
// cursor: cx (coluna), cy (linha), co (offset no buffer absoluto~)
// top: primeira linha visível na janela (scroll vertical)
//
// Undo: stack circular de operações (pos, char, tipo/is_ins)
// Clipboard: clip[] com clip_len (cut/copy/paste entre sessões~)
//
// Key bindings (^ = Ctrl):
//   ^O save, ^X exit, ^G help, ^F find, ^R replace
//   ^J go line, ^Z undo, ^W cut word, ^Y paste, ^K kill line
//   ^T swap buffer, ^C command mode, F2 toggle line numbers

#include <tui.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAXLNS   4096
#define ROWS     23
#define COLS     80
#define TABW     4

// ~~ Estado global do editor ~~
// buf: buffer de texto (char* dinâmico)
// cap/sz: capacidade e tamanho atual do buffer
// lns[]: offsets de início de cada linha (array de int)
// nlns: número de linhas
// cx/cy/co: cursor (coluna, linha, offset absoluto)
// top: primeira linha visível na janela
// fname: nome do arquivo atual
// mod: dirty flag (precisa salvar?)
// overwrite: modo overwrite (vs insert)
// msg/msg_age: mensagem na barra de status
// show_help/show_linenos: flags de UI
static char *buf;
static int  cap, sz;
static int  lns[MAXLNS], nlns;
static int  cx, cy, co;
static int  top;
static char fname[256];
static int  mod;
static int  overwrite;
static char msg[80];
static int  msg_age;
static int  show_help;
static int  show_linenos = 1;

static tui_t *tui;
static tui_win_t *w_text;
static tui_win_t *w_status;
static tui_win_t *w_help;

static void ins(int off, char c);
static void del(int off);

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int grow(int needed) {
    if (needed <= cap) return 0;
    int newcap = cap ? cap : 4096;
    while (newcap < needed) {
        if (newcap > 0x3FFFFFFF) return -1;
        newcap *= 2;
    }
    char *nb = realloc(buf, newcap);
    if (!nb) return -1;
    buf = nb;
    cap = newcap;
    return 0;
}

#define UNDO_MAX 512
static struct { int pos; char ch; int is_ins; } undo_stack[UNDO_MAX];
static int undo_head = 0;
static int undo_count = 0;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void push_undo(int pos, char ch, int is_ins) {
    if (undo_count < UNDO_MAX) {
        int i = (undo_head + undo_count) % UNDO_MAX;
        undo_stack[i].pos = pos;
        undo_stack[i].ch = ch;
        undo_stack[i].is_ins = is_ins;
        undo_count++;
    } else {
        undo_head = (undo_head + 1) % UNDO_MAX;
        int i = (undo_head + undo_count - 1) % UNDO_MAX;
        undo_stack[i].pos = pos;
        undo_stack[i].ch = ch;
        undo_stack[i].is_ins = is_ins;
    }
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int undo_one(void) {
    if (undo_count <= 0) return 0;
    undo_count--;
    int i = (undo_head + undo_count) % UNDO_MAX;
    int p = undo_stack[i].pos;
    char ch = undo_stack[i].ch;
    if (undo_stack[i].is_ins) {
        if (p >= 0 && p <= sz) { del(p); return 1; }
    } else {
        if (grow(sz + 1) == 0) {
            for (int j = sz; j > p; j--) buf[j] = buf[j - 1];
            buf[p] = ch; sz++; return 1;
        }
    }
    return 0;
}

static char clip[4096];
static int clip_len = 0;

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void clip_set(const char *s, int n) {
    if (n > 4095) n = 4095;
    memcpy(clip, s, n); clip_len = n;
}

/* ~ essa demorou pra debugar, respeita ~ */
static void sts(const char *s) {
    strncpy(msg, s, 79); msg[79] = 0; msg_age = 0;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void lns_build(void) {
    nlns = 0; lns[nlns++] = 0;
    for (int i = 0; i < sz; i++)
        if (buf[i] == '\n') { lns[nlns++] = i + 1; if (nlns >= MAXLNS) break; }
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int off_xy(int off, int *x, int *y) {
    if (off < 0) off = 0;
    if (off > sz) off = sz;
    for (int i = 0; i < nlns; i++) {
        int nl = (i + 1 < nlns) ? lns[i + 1] - 1 : sz;
        if (off <= nl || i + 1 >= nlns) { *y = i; *x = off - lns[i]; return 0; }
    }
    *y = nlns - 1; *x = 0; return 0;
}

/* ~ essa demorou pra debugar, respeita ~ */
static int xy_off(int x, int y) {
    if (y < 0) y = 0; if (y >= nlns) y = nlns - 1;
    int o = lns[y] + x;
    int nl = (y + 1 < nlns) ? lns[y + 1] - 1 : sz;
    if (o > nl) o = nl; if (o < lns[y]) o = lns[y];
    return o;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void ins(int off, char c) {
    if (grow(sz + 1) < 0) return;
    push_undo(off, c, 1);
    for (int i = sz; i > off; i--) buf[i] = buf[i - 1];
    buf[off] = c; sz++; mod = 1;
}

/* ~ essa demorou pra debugar, respeita ~ */
static void del(int off) {
    if (off < 0 || off >= sz) return;
    push_undo(off, buf[off], 0);
    for (int i = off; i < sz - 1; i++) buf[i] = buf[i + 1];
    sz--; mod = 1;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void load(const char *fn) {
    strncpy(fname, fn, 255); fname[255] = '\0';
    struct stat st;
    if (stat(fn, &st) < 0) { sz = 0; mod = 0; lns_build(); return; }
    if (st.st_size > 0x3FFFFFFF) { sts("File too large"); sz = 0; mod = 0; lns_build(); return; }
    if (grow(st.st_size + 4096) < 0) { sz = 0; mod = 0; lns_build(); return; }
    sz = 0;
    FILE *f = fopen(fn, "r");
    if (!f) sz = 0;
    else { sz = fread(buf, 1, cap - 1, f); fclose(f); }
    mod = 0;
    lns_build();
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void save(void) {
    FILE *f = fopen(fname, "w");
    if (!f) { sts("Save failed"); return; }
    fwrite(buf, 1, sz, f);
    fclose(f);
    mod = 0;
    sts("Saved");
}

/* ~ cuidado que essa aqui morde ~ */
static int find(const char *q, int from) {
    int ql = strlen(q);
    if (!ql) return 0;
    for (int i = from; i <= sz - ql; i++) {
        int match = 1;
        for (int j = 0; j < ql; j++) if (buf[i + j] != q[j]) { match = 0; break; }
        if (match) {
            co = i; off_xy(co, &cx, &cy);
            if (cy < top) top = cy;
            if (cy >= top + ROWS) top = cy - ROWS + 1;
            return 1;
        }
    }
    sts("Not found"); return 0;
}

/* ~ cuidado que essa aqui morde ~ */
static int replace(const char *q, const char *r) {
    int ql = strlen(q), rl = strlen(r);
    if (!ql) return 0;
    int count = 0;
    for (int i = 0; i <= sz - ql; i++) {
        int match = 1;
        for (int j = 0; j < ql; j++) if (buf[i + j] != q[j]) { match = 0; break; }
        if (match) {
            for (int j = 0; j < ql; j++) del(i);
            for (int j = 0; j < rl; j++) ins(i + j, r[j]);
            i += rl - 1;
            count++; mod = 1;
        }
    }
    lns_build();
    return count;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int line_indent(int line) {
    int st = lns[line];
    int en = (line + 1 < nlns) ? lns[line + 1] - 1 : sz;
    int n = 0;
    for (int i = st; i < en && i < sz; i++) {
        if (buf[i] == ' ') n++;
        else if (buf[i] == '\t') n += TABW;
        else break;
    }
    return n;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int find_match(int pos) {
    if (pos < 0 || pos >= sz) return -1;
    char c = buf[pos];
    char open, close;
    int dir;
    if (c == '(') { open = '('; close = ')'; dir = 1; }
    else if (c == ')') { open = '('; close = ')'; dir = -1; }
    else if (c == '[') { open = '['; close = ']'; dir = 1; }
    else if (c == ']') { open = '['; close = ']'; dir = -1; }
    else if (c == '{') { open = '{'; close = '}'; dir = 1; }
    else if (c == '}') { open = '{'; close = '}'; dir = -1; }
    else return -1;
    int depth = 1;
    int i = pos + dir;
    while (i >= 0 && i < sz) {
        if (buf[i] == open) depth++;
        if (buf[i] == close) depth--;
        if (depth == 0) return i;
        i += dir;
    }
    return -1;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int is_kw_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int is_kw_cont(char c) { return is_kw_start(c) || (c >= '0' && c <= '9'); }

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int is_c_keyword(const char *s, int len) {
    static const char *kw[] = {
        "auto","break","case","char","const","continue","default","do",
        "double","else","enum","extern","float","for","goto","if","int",
        "long","register","return","short","signed","sizeof","static",
        "struct","switch","typedef","union","unsigned","void","volatile","while",
        "#include","#define","#ifdef","#ifndef","#endif","#if","#else","#elif",0
    };
    for (int i = 0; kw[i]; i++) {
        int kl = strlen(kw[i]);
        if (kl == len && strncmp(s, kw[i], kl) == 0) return 1;
    }
    return 0;
}

/* Syntax highlight colors */
#define C_NORMAL TUI_WHITE, TUI_BLACK
#define C_STRING TUI_GREEN, TUI_BLACK
#define C_COMMENT TUI_RED, TUI_BLACK
#define C_PREPROC TUI_CYAN, TUI_BLACK
#define C_NUMBER TUI_MAGENTA, TUI_BLACK
#define C_KEYWORD TUI_YELLOW, TUI_BLACK
#define C_LINENO TUI_LIGHT_GRAY, TUI_BLACK

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void screen(void) {
    tui_win_clear(w_text);
    int gutter = 0;
    if (show_linenos) {
        int t = nlns > 1 ? nlns : 1;
        int w = 1;
        while (t >= 10) { w++; t /= 10; }
        gutter = w + 1;
    }
    int match_pos = find_match(co);
    for (int r = 0; r < ROWS; r++) {
        int li = top + r;
        if (li >= nlns) continue;
        int st = lns[li];
        int en = (li + 1 < nlns) ? lns[li + 1] - 1 : sz;
        tui_win_gotoxy(w_text, r, 0);
        int col = 0;
        if (show_linenos) {
            char lbuf[12]; int lw = 0, lt = li;
            if (lt == 0) lbuf[lw++] = '0';
            else do { lbuf[lw++] = '0' + (lt % 10); lt /= 10; } while (lt);
            for (int i = lw - 1; i >= 0; i--) {
                tui_win_addch_attr(w_text, lbuf[i], C_LINENO);
                col++;
            }
            tui_win_addch_attr(w_text, ' ', C_LINENO);
            col++;
        }
        int hl = 0;
        int content_col = col;
        for (int i = st; i <= en && col < COLS; i++) {
            char c = buf[i];
            if (c == '\t') {
                int sp = TABW - (col % TABW);
                while (sp-- > 0 && col < COLS) {
                    tui_win_addch_attr(w_text, ' ', C_NORMAL);
                    col++;
                }
                continue;
            }
            if (hl == 0 && c == '"') { hl = 1; tui_win_addch_attr(w_text, c, C_STRING); col++; continue; }
            if (hl == 1 && c == '"') { tui_win_addch_attr(w_text, c, C_STRING); col++; hl = 0; continue; }
            if (hl == 1 && c == '\\' && i+1 <= en) {
                tui_win_addch_attr(w_text, c, C_STRING); col++; i++;
                if (col < COLS) { tui_win_addch_attr(w_text, buf[i], C_STRING); col++; }
                continue;
            }
            if (hl == 1) { tui_win_addch_attr(w_text, c, C_STRING); col++; continue; }
            if (hl == 3 && c == '*' && i+1 <= en && buf[i+1] == '/') {
                tui_win_addch_attr(w_text, c, C_COMMENT); col++; i++;
                if (col < COLS) { tui_win_addch_attr(w_text, buf[i], C_COMMENT); col++; }
                hl = 0; continue;
            }
            if (hl == 3) { tui_win_addch_attr(w_text, c, C_COMMENT); col++; continue; }
            if (c == '/' && i+1 <= en && buf[i+1] == '/') {
                hl = 2;
                tui_win_addch_attr(w_text, c, C_COMMENT); col++;
                continue;
            }
            if (c == '/' && i+1 <= en && buf[i+1] == '*') {
                hl = 3;
                tui_win_addch_attr(w_text, c, C_COMMENT); col++;
                continue;
            }
            if (hl == 2) { tui_win_addch_attr(w_text, c, C_COMMENT); col++; continue; }
            if (c == '#' && col <= content_col + 1) {
                hl = 4;
                tui_win_addch_attr(w_text, c, C_PREPROC); col++;
                continue;
            }
            if (hl == 4) { tui_win_addch_attr(w_text, c, C_PREPROC); col++; continue; }
            if (c >= '0' && c <= '9' && !is_kw_start(c) && col < COLS) {
                tui_win_addch_attr(w_text, c, C_NUMBER); col++;
                while (i+1 <= en && col < COLS) {
                    char nc = buf[i+1];
                    if ((nc >= '0' && nc <= '9') || nc == 'x' || nc == 'X' ||
                        nc == 'a' || nc == 'f' || nc == 'A' || nc == 'F') {
                        i++; tui_win_addch_attr(w_text, buf[i], C_NUMBER); col++;
                    } else break;
                }
                continue;
            }
            if (is_kw_start(c)) {
                int ks = i;
                while (i+1 <= en && is_kw_cont(buf[i+1]) && col + (i - ks + 1) < COLS) i++;
                int kwlen = i - ks + 1;
                int fg = is_c_keyword(&buf[ks], kwlen) ? C_KEYWORD : TUI_WHITE;
                for (int j = 0; j < kwlen && col < COLS; j++) {
                    tui_win_addch_attr(w_text, buf[ks + j], fg, TUI_BLACK);
                    col++;
                }
                continue;
            }
            tui_win_addch_attr(w_text, c, C_NORMAL);
            col++;
        }
    }

    if (match_pos >= 0) {
        int mx, my;
        off_xy(co, &mx, &my);
        int sr = my - top;
        if (sr >= 0 && sr < ROWS) {
            tui_win_gotoxy(w_text, sr, mx + gutter);
            tui_win_addch_attr(w_text, buf[co], TUI_WHITE, TUI_BLUE);
        }
        off_xy(match_pos, &mx, &my);
        sr = my - top;
        if (sr >= 0 && sr < ROWS) {
            tui_win_gotoxy(w_text, sr, mx + gutter);
            tui_win_addch_attr(w_text, buf[match_pos], TUI_WHITE, TUI_BLUE);
        }
    }

    tui_win_clear(w_status);
    tui_win_color(w_status, TUI_WHITE, TUI_BLUE);
    char sb[COLS + 1]; int l = 0;
    if (msg[0]) {
        strncpy(sb, msg, COLS); sb[COLS] = 0; l = strlen(sb);
    } else {
        l = strlen(fname); if (l > COLS) l = COLS; memcpy(sb, fname, l);
        if (mod && l < COLS) sb[l++] = '*';
        char tmp[16];
        if (l < COLS) { sb[l++] = ' '; }
        int n = cx + 1, nl = 0;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        if (l < COLS) { sb[l++] = '/'; }
        n = cy + 1; nl = 0;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        if (l < COLS) { sb[l++] = ' '; sb[l++] = 'L'; sb[l++] = 'n'; }
        n = nlns; nl = 0;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        sb[l] = 0;
    }
    while (l < COLS) sb[l++] = ' ';
    sb[l] = 0;
    tui_win_gotoxy(w_status, 0, 0);
    tui_win_addstr(w_status, sb);
    /* Mode indicator */
    char mode[8]; int ml = 0;
    mode[ml++] = ' ';
    if (overwrite) { mode[ml++] = 'O'; mode[ml++] = 'V'; mode[ml++] = 'R'; }
    else { mode[ml++] = 'I'; mode[ml++] = 'N'; mode[ml++] = 'S'; }
    mode[ml] = 0;
    tui_win_gotoxy(w_status, 0, COLS - 6);
    tui_win_addstr(w_status, mode);

    tui_win_fill(w_help, ' ');
    tui_win_color(w_help, TUI_WHITE, TUI_BLACK);
    tui_win_gotoxy(w_help, 0, 0);
    if (show_help) {
        tui_win_addstr(w_help, "^O Save  ^X Exit  ^G Help  ^S Replace  ^F Find  ^C Cmd  ^Z Undo  ^J GoLn  ^W Cut  ^Y Paste  ^K Kill  F2 Ln#");
    } else {
        tui_win_addstr(w_help, "^O Save  ^X Exit  ^G Help  ^F Find  ^C Cmd");
    }

    tui_refresh(tui);

    int sr = cy - top;
    if (sr < 0) sr = 0; if (sr >= ROWS) sr = ROWS - 1;
    char esc[16]; int en = 0;
    int v = sr + 1;
    esc[en++] = 0x1B; esc[en++] = '[';
    char vb[8]; int vn = 0;
    if (v == 0) vb[vn++] = '0';
    else do { vb[vn++] = '0' + (v % 10); v /= 10; } while (v);
    while (vn > 0) esc[en++] = vb[--vn];
    esc[en++] = ';';
    int vc = cx + gutter + 1; vn = 0;
    if (vc == 0) vb[vn++] = '0';
    else do { vb[vn++] = '0' + (vc % 10); vc /= 10; } while (v);
    while (vn > 0) esc[en++] = vb[--vn];
    esc[en++] = 'H';
    write(1, esc, en);
}

/* ~ cuidado que essa aqui morde ~ */
static int prompt(const char *q, char *out, int max) {
    int p = 0;
    memset(out, 0, max);
    while (1) {
        tui_win_clear(w_status);
        tui_win_color(w_status, TUI_WHITE, TUI_BLUE);
        tui_win_gotoxy(w_status, 0, 0);
        tui_win_addstr(w_status, q);
        if (p > 0) tui_win_addstr(w_status, out);
        tui_refresh(tui);
        int k = tui_getch(tui);
        if (k == TUI_KEY_ENTER) { out[p] = 0; return p; }
        if (k == TUI_KEY_ESC) { out[0] = 0; return -1; }
        if ((k == TUI_KEY_BS || k == 127) && p > 0) { p--; out[p] = 0; }
        if (k >= 32 && k <= 126 && p < max - 1) { out[p++] = k; out[p] = 0; }
    }
}

static char *buf2;
static int cap2, sz2;
static int lns2[MAXLNS], nlns2;
static int cx2, cy2, co2, top2, mod2;
static char fn2[256];
static int active_buf = 0;

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void swap_buf(void) {
    char *tmp_buf = buf; buf = buf2; buf2 = tmp_buf;
    int tmp_cap = cap; cap = cap2; cap2 = tmp_cap;
    int tmp_sz = sz; sz = sz2; sz2 = tmp_sz;
    memcpy(lns2, lns, sizeof(lns)); nlns2 = nlns;
    cx2 = cx; cy2 = cy; co2 = co; top2 = top; mod2 = mod;
    strncpy(fn2, fname, 255); fn2[255] = '\0';
    memcpy(lns, lns2, sizeof(lns)); nlns = nlns2;
    cx = cx2; cy = cy2; co = co2; top = top2; mod = mod2;
    strncpy(fname, fn2, 255); fname[255] = '\0';
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void kill_line(void) {
    if (cy >= nlns) return;
    int st = lns[cy];
    int en = (cy + 1 < nlns) ? lns[cy + 1] - 1 : sz;
    clip_len = 0;
    for (int i = st; i <= en && i < sz && clip_len < 4095; i++)
        clip[clip_len++] = buf[i];
    clip[clip_len] = 0;
    for (int i = st; i <= en && i < sz; i++) push_undo(st, buf[st], 0);
    int n = en - st + 1;
    if (n > sz) n = sz;
    for (int i = st; i + n < sz; i++) buf[i] = buf[i + n];
    sz -= n; mod = 1;
    lns_build();
    co = xy_off(cx, cy);
    if (co > sz) co = sz;
}

/* ~~ MAIN~~ o ponto de partida dessa bagunça! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int main(int argc, char **argv) {
    tui = tui_init();
    if (!tui) return 1;
    w_text   = tui_win_new(tui, 0, 0, 80, 23, 0);
    w_status = tui_win_new(tui, 0, 23, 80, 1, 0);
    w_help   = tui_win_new(tui, 0, 24, 80, 1, 0);
    if (!w_text || !w_status || !w_help) { tui_end(tui); return 1; }

    write(1, "\x1b[?25l", 6);
    if (argc > 1) load(argv[1]); else { sz = 0; lns_build(); sts("New file"); }
    off_xy(co, &cx, &cy);
    while (1) {
        screen();
        int k = tui_getch(tui);
        msg_age++;
        if (msg_age > 1000000) msg[0] = 0;
        if (k == 'X' - 64) break;
        if (k == 'G' - 64) {
            show_help = !show_help;
            sts(show_help ? "Help shown" : "");
            continue;
        }
        if (k == 'O' - 64) { save(); continue; }
        if (k == 'F' - 64) {
            char q[128];
            if (prompt("Find: ", q, 127) > 0) find(q, co);
            continue;
        }
        if (k == 'R' - 64) {
            char q[128], r[128];
            if (prompt("Replace: ", q, 127) <= 0) continue;
            if (prompt("With: ", r, 127) > 0) {
                int n = replace(q, r);
                char tmp[32];
                sprintf(tmp, "%d replaced", n);
                sts(tmp);
            }
            continue;
        }
        if (k == 'J' - 64) {
            char q[16];
            if (prompt("Go line: ", q, 15) > 0) {
                int n = atoi(q) - 1;
                if (n < 0) n = 0;
                if (n >= nlns) n = nlns - 1;
                cy = n; cx = 0; co = lns[cy];
                if (cy < top) top = cy;
                if (cy >= top + ROWS) top = cy - ROWS + 1;
            }
            continue;
        }
        if (k == 'Z' - 64) {
            if (undo_one()) { lns_build(); off_xy(co, &cx, &cy); sts("Undo"); }
            else sts("Nothing to undo");
            continue;
        }
        if (k == 'W' - 64) {
            if (co < sz) {
                int end = co;
                while (end < sz && buf[end] == ' ') end++;
                while (end < sz && buf[end] != ' ' && buf[end] != '\n') end++;
                clip_len = 0;
                for (int i = co; i < end && clip_len < 4095; i++) clip[clip_len++] = buf[i];
                clip[clip_len] = 0;
                for (int i = co; i < end; i++) del(co);
                lns_build(); off_xy(co, &cx, &cy);
            }
            continue;
        }
        if (k == 'Y' - 64) {
            for (int i = 0; i < clip_len; i++) ins(co + i, clip[i]);
            co += clip_len; cx += clip_len;
            lns_build();
            if (cx >= COLS) { cx = 0; cy++; }
            continue;
        }
        if (k == 'K' - 64) {
            kill_line();
            if (cy >= nlns && cy > 0) cy--;
            if (co > sz) co = sz;
            continue;
        }
        if (k == 'T' - 64) {
            swap_buf();
            sts(active_buf ? "Buffer 1" : "Buffer 2");
            active_buf = !active_buf;
            continue;
        }
        if (k == 'C' - 64) {
            char cmd[128];
            if (prompt("Cmd: ", cmd, 127) > 0) {
                if (strcmp(cmd, "q") == 0) break;
                if (strncmp(cmd, "w ", 2) == 0) {
                    strncpy(fname, cmd + 2, 255); fname[255] = '\0';
                    save();
                }
                sts(cmd);
            }
            continue;
        }
        switch (k) {
            case TUI_KEY_F2:
                show_linenos = !show_linenos;
                sts(show_linenos ? "Line numbers on" : "Line numbers off");
                break;
            case TUI_KEY_INS:
                overwrite = !overwrite;
                sts(overwrite ? "Overwrite" : "Insert");
                break;
            case TUI_KEY_UP:     if (cy > 0) { cy--; co = xy_off(cx, cy); } break;
            case TUI_KEY_DOWN:   if (cy + 1 < nlns) { cy++; co = xy_off(cx, cy); } break;
            case TUI_KEY_LEFT:   if (co > 0) { co--; off_xy(co, &cx, &cy); } break;
            case TUI_KEY_RIGHT:  if (co < sz) { co++; off_xy(co, &cx, &cy); } break;
            case TUI_KEY_HOME:   cx = 0; co = lns[cy]; break;
            case TUI_KEY_END:
                co = (cy + 1 < nlns) ? lns[cy + 1] - 1 : sz;
                if (co < lns[cy]) co = lns[cy];
                off_xy(co, &cx, &cy);
                break;
            case TUI_KEY_PGUP:
                top -= ROWS; if (top < 0) top = 0;
                cy -= ROWS; if (cy < 0) cy = 0;
                co = xy_off(cx, cy);
                break;
            case TUI_KEY_PGDN:
                top += ROWS; if (top + ROWS > nlns) top = nlns < ROWS ? 0 : nlns - ROWS;
                if (top < 0) top = 0;
                cy += ROWS; if (cy >= nlns) cy = nlns - 1;
                co = xy_off(cx, cy);
                break;
            case TUI_KEY_DEL:
                if (co < sz) { del(co); lns_build(); co = xy_off(cx, cy); }
                break;
            case TUI_KEY_BS:
                if (co > 0) { del(co - 1); co--; lns_build(); off_xy(co, &cx, &cy); }
                break;
            case TUI_KEY_ENTER: {
                ins(co, '\n');
                int ind = 0;
                if (cy + 1 < nlns) ind = line_indent(cy);
                for (int i = 0; i < ind; i++) ins(co + 1 + i, ' ');
                int new_off = co + 1 + ind;
                lns_build();
                off_xy(new_off, &cx, &cy);
                if (cy < top) top = cy;
                if (cy >= top + ROWS) top = cy - ROWS + 1;
                break;
            }
            case TUI_KEY_TAB: {
                int sp = TABW - (cx % TABW);
                if (sp <= 0) sp = TABW;
                for (int i = 0; i < sp; i++) ins(co + i, ' ');
                co += sp; cx += sp;
                if (cx >= COLS) { cx = 0; cy++; }
                lns_build();
                break;
            }
            default:
                if (k >= 32 && k <= 126) {
                    if (overwrite && co < sz && buf[co] != '\n') {
                        buf[co] = k; mod = 1; co++; cx++;
                    } else {
                        ins(co, k); co++; cx++;
                        lns_build();
                    }
                    if (cx >= COLS) { cx = 0; cy++; }
                    if (cy < top) top = cy;
                    if (cy >= top + ROWS) top = cy - ROWS + 1;
                }
                break;
        }
        if (co > sz) co = sz; if (co < 0) co = 0;
    }
    tui_end(tui);
    return 0;
}




/* ♥ graphy.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
