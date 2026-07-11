#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#define KBUF     65536
#define MAXLNS   4096
#define ROWS     23
#define COLS     80
#define TABW     4

enum {
    K_ESC=27, K_UP=300, K_DOWN, K_LEFT, K_RIGHT,
    K_HOME, K_END, K_PGUP, K_PGDN, K_INS, K_DEL,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
    K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_BS=127, K_TAB=9, K_ENTER=10
};

static char buf[KBUF];
static int  sz;
static int  lns[MAXLNS], nlns;
static int  cx, cy, co;
static int  top;
static char fname[256];
static int  mod;
static int  overwrite;
static char msg[80];
static int  msg_age;
static int  show_help;

static void ins(int off, char c);
static void del(int off);

// ─── Undo system ───────────────────────────────────────
#define UNDO_MAX 512
static struct { int pos; char ch; int is_ins; } undo_stack[UNDO_MAX];
static int undo_head = 0;
static int undo_count = 0;

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

static int undo_one(void) {
    if (undo_count <= 0) return 0;
    undo_count--;
    int i = (undo_head + undo_count) % UNDO_MAX;
    int p = undo_stack[i].pos;
    char ch = undo_stack[i].ch;
    if (undo_stack[i].is_ins) {
        if (p >= 0 && p <= sz) { del(p); return 1; }
    } else {
        if (sz < KBUF - 1) {
            for (int j = sz; j > p; j--) buf[j] = buf[j - 1];
            buf[p] = ch; sz++; return 1;
        }
    }
    return 0;
}

// ─── Clipboard ──────────────────────────────────────────
static char clip[4096];
static int clip_len = 0;

static void clip_set(const char *s, int n) {
    if (n > 4095) n = 4095;
    memcpy(clip, s, n); clip_len = n;
}

// ─── helpers ──────────────────────────────────────────
static void sts(const char *s) {
    strncpy(msg, s, 79); msg[79] = 0; msg_age = 0;
}

static void lns_build(void) {
    nlns = 0; lns[nlns++] = 0;
    for (int i = 0; i < sz && i < KBUF; i++)
        if (buf[i] == '\n') { lns[nlns++] = i + 1; if (nlns >= MAXLNS) break; }
}

static int off_xy(int off, int *x, int *y) {
    if (off < 0) off = 0;
    if (off > sz) off = sz;
    for (int i = 0; i < nlns; i++) {
        int nl = (i + 1 < nlns) ? lns[i + 1] - 1 : sz;
        if (off <= nl || i + 1 >= nlns) { *y = i; *x = off - lns[i]; return 0; }
    }
    *y = nlns - 1; *x = 0; return 0;
}

static int xy_off(int x, int y) {
    if (y < 0) y = 0; if (y >= nlns) y = nlns - 1;
    int o = lns[y] + x;
    int nl = (y + 1 < nlns) ? lns[y + 1] - 1 : sz;
    if (o > nl) o = nl; if (o < lns[y]) o = lns[y];
    return o;
}

static void ins(int off, char c) {
    if (sz >= KBUF - 1) return;
    push_undo(off, c, 1);
    for (int i = sz; i > off; i--) buf[i] = buf[i - 1];
    buf[off] = c; sz++; mod = 1;
}

static void del(int off) {
    if (off < 0 || off >= sz) return;
    push_undo(off, buf[off], 0);
    for (int i = off; i < sz - 1; i++) buf[i] = buf[i + 1];
    sz--; mod = 1;
}

static int rd_k(void) {
    char c = getchar();
    if (c != '\x1b') return (unsigned char)c;
    for (int wait = 0; wait < 50000; wait++) {
        if (kbhit()) {
            char s[8]; s[0] = '\x1b';
            int n = 1;
            while (n < 7) {
                if (!kbhit()) break;
                s[n++] = getchar();
                if (s[n-1] >= 0x40 && s[n-1] <= 0x7e && s[1] != '[') break;
                if (s[n-1] == '~') break;
                if (n >= 2 && s[1] == '[' && s[n-1] >= 0x40 && s[n-1] <= 0x7e) break;
                if (n >= 2 && s[1] == 'O' && s[n-1] >= 0x40 && s[n-1] <= 0x7e) break;
            }
            if (n == 2) {
                if (s[1] == 'H') return K_HOME;
                if (s[1] == 'F') return K_END;
                if (s[1] >= 'A' && s[1] <= 'D') return K_UP + (s[1] - 'A');
            }
            if (n >= 3 && s[1] == '[') {
                if (s[2] >= 'A' && s[2] <= 'D') return K_UP + (s[2] - 'A');
                if (s[2] == 'H') return K_HOME;
                if (s[2] == 'F') return K_END;
                if (s[2] == '2' && s[3] == '~') return K_INS;
                if (s[2] == '3' && s[3] == '~') return K_DEL;
                if (s[2] == '5' && s[3] == '~') return K_PGUP;
                if (s[2] == '6' && s[3] == '~') return K_PGDN;
                if (s[2] == '1' && n >= 5) {
                    if (s[3] == '5' && s[4] == '~') return K_F5;
                    if (s[3] == '7' && s[4] == '~') return K_F6;
                    if (s[3] == '8' && s[4] == '~') return K_F7;
                    if (s[3] == '9' && s[4] == '~') return K_F8;
                }
                if (s[2] == '2' && n >= 5) {
                    if (s[3] == '0' && s[4] == '~') return K_F9;
                    if (s[3] == '1' && s[4] == '~') return K_F10;
                    if (s[3] == '3' && s[4] == '~') return K_F11;
                    if (s[3] == '4' && s[4] == '~') return K_F12;
                }
            }
            if (n >= 2 && s[1] == 'O' && n >= 3) {
                if (s[2] == 'P') return K_F1; if (s[2] == 'Q') return K_F2;
                if (s[2] == 'R') return K_F3; if (s[2] == 'S') return K_F4;
            }
            return K_ESC;
        }
    }
    return K_ESC;
}

static void pr_status(const char *s) {
    write(1, "\x1b[7m", 4);
    int sl = strlen(s); if (sl > COLS) sl = COLS;
    write(1, s, sl);
    for (int i = sl; i < COLS; i++) write(1, " ", 1);
    write(1, "\x1b[m", 3);
}

static int prompt(const char *q, char *out, int max) {
    int p = 0;
    memset(out, 0, max);
    while (1) {
        pr_status(q);
        if (p > 0) write(1, out, p);
        int k = rd_k();
        if (k == '\n') { out[p] = 0; return p; }
        if (k == K_ESC) { out[0] = 0; return -1; }
        if ((k == K_BS || k == 127) && p > 0) { p--; out[p] = 0; }
        if (k >= 32 && k <= 126 && p < max - 1) { out[p++] = k; out[p] = 0; }
    }
}

static void load(const char *fn) {
    strncpy(fname, fn, 255);
    struct stat st;
    if (stat(fn, &st) < 0) { sz = 0; mod = 0; lns_build(); return; }
    sz = 0;
    FILE *f = fopen(fn, "r");
    if (!f) sz = 0;
    else { sz = fread(buf, 1, KBUF - 1, f); fclose(f); }
    mod = 0;
    lns_build();
}

static void save(void) {
    FILE *f = fopen(fname, "w");
    if (!f) { sts("Save failed"); return; }
    fwrite(buf, 1, sz, f);
    fclose(f);
    mod = 0;
    sts("Saved");
}

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
            count++;
            mod = 1;
        }
    }
    lns_build();
    return count;
}

// ─── Auto-indent ──────────────────────────────────────
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

// ─── Syntax highlighting ──────────────────────────────
static void wc(const char *s, int n) { write(1, s, n); }

static int is_kw_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_kw_cont(char c) { return is_kw_start(c) || (c >= '0' && c <= '9'); }

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

static void screen(void) {
    write(1, "\x1b[H", 3);
    for (int r = 0; r < ROWS; r++) {
        int li = top + r;
        if (li >= nlns) { write(1, "\x1b[K\n", 4); continue; }
        int st = lns[li];
        int en = (li + 1 < nlns) ? lns[li + 1] - 1 : sz;
        // Line number
        char lbuf[12]; int lw = 0, lt = li;
        if (lt == 0) lbuf[lw++] = '0';
        else do { lbuf[lw++] = '0' + (lt % 10); lt /= 10; } while (lt);
        write(1, "\x1b[37m", 4); // gray line numbers
        for (int i = lw - 1; i >= 0; i--) write(1, &lbuf[i], 1);
        write(1, "\x1b[m ", 3);
        int col = lw + 1;
        // Syntax highlighting state machine
        int hl = 0; // 0=normal, 1=string, 2=line_cmt, 3=block_cmt, 4=preproc
        int buf_start = -1; // start of preproc or keyword
        for (int i = st; i <= en && col < COLS; i++) {
            char c = buf[i];
            if (c == '\t') {
                int sp = TABW - (col % TABW);
                while (sp-- > 0 && col < COLS) { write(1, " ", 1); col++; }
                continue;
            }
            // State transitions
            if (hl == 0 && c == '"') { hl = 1; wc("\x1b[32m",4); wc(&c,1); col++; continue; }
            if (hl == 1 && c == '"') { wc(&c,1); col++; hl = 0; wc("\x1b[m",3); continue; }
            if (hl == 1 && c == '\\' && i+1 <= en) { wc(&c,1); col++; i++; if (col < COLS) { wc(&buf[i],1); col++; } continue; }
            if (hl == 1) { wc(&c,1); col++; continue; }
            if (hl == 3 && c == '*' && i+1 <= en && buf[i+1] == '/') {
                wc(&c,1); col++; i++; if (col < COLS) { wc(&buf[i],1); col++; }
                hl = 0; wc("\x1b[m",3); continue;
            }
            if (hl == 3) { wc(&c,1); col++; continue; }
            if (c == '/' && i+1 <= en && buf[i+1] == '/') { hl = 2; wc("\x1b[31m",4); wc(&c,1); col++; continue; }
            if (c == '/' && i+1 <= en && buf[i+1] == '*') { hl = 3; wc("\x1b[31m",4); wc(&c,1); col++; continue; }
            if (hl == 2 && c == '\n') { hl = 0; wc("\x1b[m",3); wc(&c,1); col++; continue; }
            if (hl == 2) { wc(&c,1); col++; continue; }
            if (c == '#' && col <= lw + 2) { hl = 4; wc("\x1b[36m",4); wc(&c,1); col++; continue; }
            if (hl == 4 && c == '\n') { hl = 0; wc("\x1b[m",3); wc(&c,1); col++; continue; }
            if (hl == 4) { wc(&c,1); col++; continue; }
            // Digits
            if (c >= '0' && c <= '9' && !is_kw_start(c) && col < COLS) {
                wc("\x1b[35m",4); wc(&c,1); col++;
                while (i+1 <= en && col < COLS) {
                    char nc = buf[i+1];
                    if ((nc >= '0' && nc <= '9') || nc == 'x' || nc == 'X' || nc == 'a' || nc == 'f' || nc == 'A' || nc == 'F') {
                        i++; wc(&buf[i],1); col++;
                    } else break;
                }
                wc("\x1b[m",3); continue;
            }
            // Keyword detection: buffer letters, check at word boundary
            if (is_kw_start(c)) {
                int ks = i;
                while (i+1 <= en && is_kw_cont(buf[i+1]) && col + (i - ks + 1) < COLS) i++;
                int kwlen = i - ks + 1;
                if (is_c_keyword(&buf[ks], kwlen)) {
                    wc("\x1b[33m",4);
                    for (int j = 0; j < kwlen && col < COLS; j++) { wc(&buf[ks+j],1); col++; }
                    wc("\x1b[m",3);
                } else {
                    for (int j = 0; j < kwlen && col < COLS; j++) { wc(&buf[ks+j],1); col++; }
                }
                continue;
            }
            wc(&c,1); col++;
        }
        wc("\x1b[K\n",4);
        if (hl == 3) wc("\x1b[m",3); // reset on newline if in block comment
    }
    // Status bar
    char sb[COLS + 1]; int l = 0;
    if (msg[0]) {
        strncpy(sb, msg, COLS); sb[COLS] = 0; l = strlen(sb);
    } else {
        l = strlen(fname); memcpy(sb, fname, l);
        if (mod && l < COLS) sb[l++] = '*';
        char tmp[16]; int nl;
        if (l < COLS) { sb[l++] = ' '; }
        nl = 0; int n = cx + 1;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        if (l < COLS) { sb[l++] = '/'; }
        nl = 0; n = cy + 1;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        if (l < COLS) { sb[l++] = ' '; sb[l++] = 'L'; sb[l++] = 'n'; }
        nl = 0; n = nlns;
        if (n == 0) tmp[nl++] = '0';
        else do { tmp[nl++] = '0' + (n % 10); n /= 10; } while (n);
        for (int i = nl - 1; i >= 0 && l < COLS; i--) sb[l++] = tmp[i];
        sb[l] = 0;
    }
    while (l < COLS) sb[l++] = ' '; sb[l] = 0;
    write(1, "\x1b[7m", 4);
    write(1, sb, COLS);
    write(1, "\x1b[m\n", 4);
    if (show_help) {
        const char *hlp = "^O Save  ^X Exit  ^G Help  ^S Replace  ^F Find  ^C Cmd  ^Z Undo  ^J GoLn  ^W Cut  ^Y Paste  ^K Kill  F2 Ln#";
        write(1, hlp, strlen(hlp));
    } else {
        const char *scut = "^O Save  ^X Exit  ^G Help  ^F Find  ^C Cmd";
        write(1, scut, strlen(scut));
    }
    for (int i = 0; i < COLS; i++) write(1, " ", 1);
    // Cursor positioning
    char esc[16]; int en = 0;
    int sr = cy - top; if (sr < 0) sr = 0; if (sr >= ROWS) sr = ROWS - 1;
    esc[en++] = '\x1b'; esc[en++] = '[';
    int v = sr + 1; char vb[8]; int vn = 0;
    if (v == 0) vb[vn++] = '0';
    else do { vb[vn++] = '0' + (v % 10); v /= 10; } while (v);
    while (vn > 0) esc[en++] = vb[--vn];
    esc[en++] = ';';
    int vc = cx + 6; vn = 0;
    if (vc == 0) vb[vn++] = '0';
    else do { vb[vn++] = '0' + (vc % 10); vc /= 10; } while (v);
    while (vn > 0) esc[en++] = vb[--vn];
    esc[en++] = 'H';
    write(1, esc, en);
}

// ─── multiple file buffers ────────────────────────────
static char buf2[KBUF];
static int sz2;
static int lns2[MAXLNS], nlns2;
static int cx2, cy2, co2, top2, mod2;
static char fn2[256];
static int active_buf = 0;

static void swap_buf(void) {
    // save current to buf2
    memcpy(buf2, buf, KBUF); sz2 = sz;
    memcpy(lns2, lns, sizeof(lns)); nlns2 = nlns;
    cx2 = cx; cy2 = cy; co2 = co; top2 = top; mod2 = mod;
    strcpy(fn2, fname);
    // load from buf2
    memcpy(buf, buf2, KBUF); sz = sz2;
    memcpy(lns, lns2, sizeof(lns)); nlns = nlns2;
    cx = cx2; cy = cy2; co = co2; top = top2; mod = mod2;
    strcpy(fname, fn2);
}

// ─── kill line / yank ─────────────────────────────────
static void kill_line(void) {
    if (cy >= nlns) return;
    int st = lns[cy];
    int en = (cy + 1 < nlns) ? lns[cy + 1] - 1 : sz;
    clip_len = 0;
    for (int i = st; i <= en && i < sz && clip_len < 4095; i++) {
        clip[clip_len++] = buf[i];
    }
    clip[clip_len] = 0;
    // delete the line content
    for (int i = st; i <= en && i < sz; i++) push_undo(st, buf[st], 0);
    int n = en - st + 1;
    if (n > sz) n = sz;
    for (int i = st; i + n < sz; i++) buf[i] = buf[i + n];
    sz -= n; mod = 1;
    lns_build();
    co = xy_off(cx, cy);
    if (co > sz) co = sz;
}

// ─── main ─────────────────────────────────────────────
int main(int argc, char **argv) {
    write(1, "\x1b[2J\x1b[?25l", 9);
    if (argc > 1) load(argv[1]); else { sz = 0; lns_build(); sts("New file"); }
    off_xy(co, &cx, &cy);
    while (1) {
        screen();
        int k = rd_k();
        msg_age++;
        if (msg_age > 1000000) msg[0] = 0;
        if (k == 'X' - 64) break;           // ^X
        if (k == 'G' - 64) {                // ^G
            show_help = !show_help;
            if (show_help) sts("Help shown");
            else sts("");
            continue;
        }
        if (k == 'O' - 64) { save(); continue; }
        if (k == 'F' - 64) {                // ^F Find
            char q[128];
            if (prompt("Find: ", q, 127) > 0) find(q, co);
            continue;
        }
        if (k == 'R' - 64) {                // ^S Replace
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
        if (k == 'J' - 64) {                // ^J Go to line
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
        if (k == 'Z' - 64) {                // ^Z Undo
            if (undo_one()) { lns_build(); off_xy(co, &cx, &cy); sts("Undo"); }
            else sts("Nothing to undo");
            continue;
        }
        if (k == 'W' - 64) {                // ^W Cut word (or selected text)
            // simple: kill current word forward
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
        if (k == 'Y' - 64) {                // ^Y Paste clipboard
            for (int i = 0; i < clip_len; i++) ins(co + i, clip[i]);
            co += clip_len; cx += clip_len;
            lns_build();
            if (cx >= COLS) { cx = 0; cy++; }
            continue;
        }
        if (k == 'K' - 64) {                // ^K Kill line
            kill_line();
            if (cy >= nlns && cy > 0) cy--;
            if (co > sz) co = sz;
            continue;
        }
        if (k == 'T' - 64) {                // ^T Toggle buffer
            swap_buf();
            sts(active_buf ? "Buffer 1" : "Buffer 2");
            active_buf = !active_buf;
            continue;
        }
        if (k == 'C' - 64) {                // ^C Command
            char cmd[128];
            if (prompt("Cmd: ", cmd, 127) > 0) {
                if (strcmp(cmd, "q") == 0) break;
                if (strncmp(cmd, "w ", 2) == 0) {
                    strncpy(fname, cmd + 2, 255);
                    save();
                }
                sts(cmd);
            }
            continue;
        }
        switch (k) {
            case K_UP:     if (cy > 0) { cy--; co = xy_off(cx, cy); } break;
            case K_DOWN:   if (cy + 1 < nlns) { cy++; co = xy_off(cx, cy); } break;
            case K_LEFT:   if (co > 0) { co--; off_xy(co, &cx, &cy); } break;
            case K_RIGHT:  if (co < sz) { co++; off_xy(co, &cx, &cy); } break;
            case K_HOME:   cx = 0; co = lns[cy]; break;
            case K_END:
                co = (cy + 1 < nlns) ? lns[cy + 1] - 1 : sz;
                if (co < lns[cy]) co = lns[cy];
                off_xy(co, &cx, &cy);
                break;
            case K_PGUP:
                top -= ROWS; if (top < 0) top = 0;
                cy -= ROWS; if (cy < 0) cy = 0;
                co = xy_off(cx, cy);
                break;
            case K_PGDN:
                top += ROWS; if (top + ROWS > nlns) top = nlns < ROWS ? 0 : nlns - ROWS;
                if (top < 0) top = 0;
                cy += ROWS; if (cy >= nlns) cy = nlns - 1;
                co = xy_off(cx, cy);
                break;
            case K_DEL:
                if (co < sz) { del(co); lns_build(); co = xy_off(cx, cy); }
                break;
            case K_BS:
                if (co > 0) { del(co - 1); co--; lns_build(); off_xy(co, &cx, &cy); }
                break;
            case K_ENTER: {
                ins(co, '\n');
                // auto-indent
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
            case K_TAB: {
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
    write(1, "\x1b[2J\x1b[H\x1b[?25h", 10);
    return 0;
}
