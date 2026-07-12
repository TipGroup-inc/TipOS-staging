#include "idt.h"
#include "memory.h"
#include "../drivers/ata.h"
#include "fat32.h"
#include "../commands/shell_cmds.h"
#include "ring3.h"
#include "kernel.h"

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read(void);
int keyboard_avail(void);
void pic_init(void);
void smc_init(void);
void nvram_init(void);

#include "colors.h"

#define VGA_ADDR  0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

uint8_t vga_attr = C_OUTPUT;

void set_vga_color(uint8_t color) { vga_attr = color; }

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}
static int serial_is_transmit_empty(void) { return inb(0x3F8 + 5) & 0x20; }
void serial_putc(char c) { while (!serial_is_transmit_empty()); outb(0x3F8, c); }
static void serial_puts(const char *s) { while (*s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); } }

volatile unsigned short *vga = (unsigned short *)VGA_ADDR;
int cx = 0, cy = 0;
static int esc_state = 0;
static int esc_params[4];
static int esc_np = 0;
static int esc_question = 0;
static int esc_rev = 0;
static int cur_visible = 1;
void vga_clear(void);

// ─── redirection globals ────────────────────────────────
static int redir_active = 0;   // 0=normal, 1=write to buffer
static char redir_buf[16384];
static int redir_len = 0;

static void vga_set_cursor(void) {
    unsigned short pos = cy * VGA_WIDTH + cx;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
}

static void vga_scroll(void) {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) vga[i] = vga[i + VGA_WIDTH];
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (vga_attr << 8) | ' ';
    cy = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    // Redirection: write to buffer instead of VGA
    if (redir_active) {
        if (redir_len < 16384) redir_buf[redir_len++] = c;
        return;
    }
    if (esc_state == 1) {
        if (c == '[') { esc_state = 2; esc_np = 0; esc_question = 0; esc_params[0] = 0; return; }
        if (c == 'A') { if (cy > 0) cy--; esc_state = 0; return; }
        if (c == 'B') { if (cy < VGA_HEIGHT-1) cy++; esc_state = 0; return; }
        if (c == 'C') { if (cx < VGA_WIDTH-1) cx++; esc_state = 0; return; }
        if (c == 'D') { if (cx > 0) cx--; esc_state = 0; return; }
        if (c == 'H') { cx = cy = 0; esc_state = 0; return; }
        esc_state = 0;
        serial_putc('\x1b'); serial_putc(c);
        vga[cy * VGA_WIDTH + cx] = (vga_attr << 8) | c; cx++;
        if (cx >= VGA_WIDTH) { cx = 0; cy++; }
        if (cy >= VGA_HEIGHT) vga_scroll();
        return;
    }
    if (esc_state == 2 || esc_state == 3) {
        if (c >= '0' && c <= '9') {
            esc_params[esc_np] = esc_params[esc_np] * 10 + (c - '0');
            esc_state = 3;
            return;
        }
        if (c == ';') { esc_np++; if (esc_np < 4) esc_params[esc_np] = 0; esc_state = 3; return; }
        if (c == '?') { esc_question = 1; return; }
        if (c == 'H' || c == 'f') {
            int r = esc_params[0] > 0 ? esc_params[0] - 1 : 0;
            int cc = esc_params[1] > 0 ? esc_params[1] - 1 : 0;
            if (r >= 0 && r < VGA_HEIGHT) cy = r;
            if (cc >= 0 && cc < VGA_WIDTH) cx = cc;
            esc_state = 0; return;
        }
        if (c == 'J') {
            if (esc_params[0] == 2) { vga_clear(); }
            esc_state = 0; return;
        }
        if (c == 'K') {
            for (int i = cx; i < VGA_WIDTH; i++) vga[cy * VGA_WIDTH + i] = (vga_attr << 8) | ' ';
            esc_state = 0; return;
        }
        if (c == 'm') {
            if (esc_params[0] == 7) esc_rev = 1;
            else if (esc_params[0] == 0 || esc_np >= 0) esc_rev = 0;
            esc_state = 0; return;
        }
        if (c == 'l' && esc_question) {
            if (esc_params[0] == 25) cur_visible = 1;
            esc_state = 0; return;
        }
        if (c == 'h' && esc_question) {
            if (esc_params[0] == 25) cur_visible = 0;
            esc_state = 0; return;
        }
        esc_state = 0; return;
    }
    if (c == '\x1b') { esc_state = 1; return; }
    if (c == '\n') {
        serial_putc('\r'); serial_putc('\n');
        cx = 0; cy++;
    } else if (c == '\b') {
        if (cx > 0) { cx--; vga[cy * VGA_WIDTH + cx] = (vga_attr << 8) | ' '; }
        serial_putc('\b');
    } else if (c == '\r') {
        cx = 0;
        serial_putc('\r');
    } else {
        unsigned short attr = (esc_rev ? 0x70 : vga_attr) << 8;
        vga[cy * VGA_WIDTH + cx] = attr | c; cx++;
        serial_putc(c);
    }
    if (cx >= VGA_WIDTH) { cx = 0; cy++; }
    if (cy >= VGA_HEIGHT) vga_scroll();
    if (cur_visible) vga_set_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}
void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = (vga_attr << 8) | ' ';
    cx = cy = 0; esc_state = 0; esc_rev = 0;
}

void debug_puts(const char *s) {
    vga_puts(s);
}

// ─── RTC / CMOS ────────────────────────────────────────
static uint8_t read_cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int cmos_bcd(int v) {
    return (v & 0x0F) + ((v / 16) * 10);
}

void rtc_read(rtc_time *t) {
    t->s = cmos_bcd(read_cmos(0x00));
    t->m = cmos_bcd(read_cmos(0x02));
    t->h = cmos_bcd(read_cmos(0x04));
    t->dy = cmos_bcd(read_cmos(0x07));
    t->mo = cmos_bcd(read_cmos(0x08));
    t->yr = cmos_bcd(read_cmos(0x09)) + 2000;
}

// ─── ^C (SIGINT) ──────────────────────────────────────
volatile int sigint_pending = 0;

// ─── PIT / Timer ───────────────────────────────────────
volatile uint64_t timer_ticks = 0;

void timer_tick_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20); // EOI
}

static void pit_init(void) {
    uint32_t div = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

static void sleep_ms(uint64_t ms) {
    uint64_t target = timer_ticks + ms / 10 + 1;
    while (timer_ticks < target) { __asm__ volatile ("pause"); }
}

// ─── num / str helpers ─────────────────────────────────
static void put_uint(uint32_t n) {
    char buf[12]; int p = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n) { buf[p++] = '0' + (n % 10); n /= 10; }
    while (p) vga_putchar(buf[--p]);
}

void *memset(void *s, int c, int n) {
    for (int i = 0; i < n; i++) ((uint8_t*)s)[i] = c;
    return s;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

int strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strncpy(char *d, const char *s, int n) { char *r = d; while (n-- > 0 && (*d++ = *s++)); while (n-- > 0) *d++ = 0; return r; }

// ─── key codes ──────────────────────────────────────────
enum { K_UP=300, K_DOWN, K_LEFT, K_RIGHT, K_HOME, K_END, K_DEL };

static int read_key(void) {
    char c = keyboard_read();
    if (c != '\x1b') return (unsigned char)c;
    if (keyboard_avail()) {
        char s2 = keyboard_read();
        if (s2 == '[' || s2 == 'O') {
            if (!keyboard_avail()) return '\x1b';
            char s3 = keyboard_read();
            if (s2 == '[') {
                if (s3 == 'A') return K_UP;    if (s3 == 'B') return K_DOWN;
                if (s3 == 'C') return K_RIGHT;  if (s3 == 'D') return K_LEFT;
                if (s3 == 'H') return K_HOME;   if (s3 == 'F') return K_END;
                if (s3 == '3') {
                    if (!keyboard_avail()) return '\x1b';
                    if (keyboard_read() == '~') return K_DEL;
                }
            }
        }
    }
    return '\x1b';
}

// ─── shell history ──────────────────────────────────────
#define HIST_MAX 128
#define CMD_MAX  256
static char history[HIST_MAX][CMD_MAX];
static int hist_count = 0;

void execute_command(const char *cmd);
static int last_exit_code = 0;

// ─── Job Table (background jobs) ───────────────────────
#define MAX_JOBS 16
#define JOB_CMD_MAX 64
static struct {
    int used;
    int jid;
    char cmd[JOB_CMD_MAX];
    int pid;
} job_table[MAX_JOBS];
static int next_pid = 100;

static int job_add(const char *cmd) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].used) {
            job_table[i].used = 1;
            job_table[i].jid = i + 1;
            int j = 0;
            while (cmd[j] && j < JOB_CMD_MAX-1) { job_table[i].cmd[j] = cmd[j]; j++; }
            job_table[i].cmd[j] = 0;
            job_table[i].pid = next_pid++;
            return job_table[i].jid;
        }
    }
    return -1;
}

static void job_remove(int jid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].used && job_table[i].jid == jid)
            { job_table[i].used = 0; return; }
}

static int job_find(int jid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].used && job_table[i].jid == jid)
            return i;
    return -1;
}

// ─── Environment Variables ──────────────────────────────
#define ENV_MAX 32
static char env_name[ENV_MAX][32];
static char env_val[ENV_MAX][256];
static int env_count = 0;

static const char *env_get(const char *name) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_name[i], name) == 0) return env_val[i];
    return NULL;
}

static void env_set(const char *name, const char *val) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_name[i], name) == 0) { strncpy(env_val[i], val, 255); return; }
    if (env_count < ENV_MAX) {
        strncpy(env_name[env_count], name, 31);
        strncpy(env_val[env_count], val, 255);
        env_count++;
    }
}

static void env_init(void) {
    env_set("PATH", "/BIN:/USR/BIN:/LOCAL/BIN");
    env_set("HOME", "/");
    env_set("EDITOR", "edit");
    env_set("SHELL", "MkM");
    env_set("PS1", "[\\w]\\$ ");
}

// ─── Aliases ────────────────────────────────────────────
#define ALIAS_MAX 32
static char alias_name[ALIAS_MAX][64];
static char alias_val[ALIAS_MAX][256];
static int alias_count = 0;

static const char *alias_get(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) return alias_val[i];
    return NULL;
}

static void alias_set(const char *name, const char *val) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) { strncpy(alias_val[i], val, 255); return; }
    if (alias_count < ALIAS_MAX) {
        strncpy(alias_name[alias_count], name, 63);
        strncpy(alias_val[alias_count], val, 255);
        alias_count++;
    }
}

static int alias_unset(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) {
            for (int j = i; j < alias_count - 1; j++) {
                strcpy(alias_name[j], alias_name[j+1]);
                strcpy(alias_val[j], alias_val[j+1]);
            }
            alias_count--; return 1;
        }
    return 0;
}

void execute_command(const char *cmd) {
    // ── alias expansion (first run only, non-recursive for safety) ────
    char first[64]; int fi = 0, ci = 0;
    while (cmd[ci] && cmd[ci] != '>' && cmd[ci] != '<' && cmd[ci] != '|' && fi < 63) {
        if (cmd[ci] != ' ') first[fi++] = cmd[ci];
        else { if (fi > 0) break; }
        ci++;
    }
    first[fi] = '\0';
    if (fi > 0) {
        const char *aexp = alias_get(first);
        if (aexp) {
            char xbuf[CMD_MAX]; int xi = 0;
            for (int i = 0; aexp[i] && xi < CMD_MAX-2; i++) xbuf[xi++] = aexp[i];
            const char *rest = cmd;
            while (*rest && *rest != ' ' && *rest != '>' && *rest != '<' && *rest != '|') rest++;
            while (*rest == ' ') rest++;
            if (*rest && xi < CMD_MAX-2) xbuf[xi++] = ' ';
            while (*rest && xi < CMD_MAX-2) xbuf[xi++] = *rest++;
            xbuf[xi] = '\0';
            execute_command(xbuf);
            return;
        }
    }

    // ── parse redirections ───────────────────────────────
    char work[CMD_MAX]; int wi = 0;
    const char *redir_file = 0;
    int redir_append = 0;
    int redir_in = 0;     // for < input file
    char in_file[CMD_MAX];
    int pipe_mode = 0;    // for |
    char pipe_cmd[CMD_MAX];

    {
        const char *p = cmd;
        while (*p && *p != '>' && *p != '<' && *p != '|') {
            if (wi < CMD_MAX-1) work[wi++] = *p;
            p++;
        }
        // trim trailing spaces
        while (wi > 0 && work[wi-1] == ' ') wi--;
        work[wi] = 0;

        if (*p == '>') {
            p++;
            if (*p == '>') { redir_append = 1; p++; }
            while (*p == ' ') p++;
            redir_file = p;
        } else if (*p == '<') {
            p++;
            while (*p == ' ') p++;
            redir_in = 1;
            int ri = 0;
            while (*p && ri < CMD_MAX-1) in_file[ri++] = *p++;
            in_file[ri] = 0;
        } else if (*p == '|') {
            p++;
            while (*p == ' ') p++;
            pipe_mode = 1;
            int ri = 0;
            while (*p && ri < CMD_MAX-1) pipe_cmd[ri++] = *p++;
            pipe_cmd[ri] = 0;
        }
    }

    // ── parse & (background job) ─────────────────────────
    int bg_job = 0;
    if (wi > 0 && work[wi-1] == '&') {
        work[--wi] = 0;
        bg_job = 1;
    }

    // ── handle < input redirection ───────────────────────
    if (redir_in) {
        static uint8_t ibuf[512];
        int n = fat32_read_file(in_file, ibuf, 512);
        if (n > 0 && n <= 512) {
            // feed file content as command to execute
            // simple: just cat the file
            for (int i = 0; i < n; i++) vga_putchar(ibuf[i]);
        } else {
            set_vga_color(C_ERROR);
            vga_puts("Erro: ");
            vga_puts(in_file);
            vga_puts(" nao encontrado\n");
            set_vga_color(C_OUTPUT);
        }
        return;
    }

    // ── handle > / >> redirection ───────────────────────
    if (redir_file) {
        redir_active = 1;
        redir_len = 0;
    }

    // ── dispatch command ─────────────────────────────────
    // builtins
    if (work[0] == 0) { /* empty */ }
    else if (strcmp(work, "help") == 0) cmd_help();
    else if (strcmp(work, "clear") == 0) cmd_clear();
    else if (strncmp(work, "echo ", 5) == 0) cmd_echo(work + 5);
    else if (strcmp(work, "about") == 0) cmd_about();
    else if (strcmp(work, "shutdown") == 0) cmd_shutdown();
    else if (strcmp(work, "reboot") == 0) cmd_reboot();
    else if (strcmp(work, "ls") == 0) cmd_ls();
    else if (strcmp(work, "date") == 0) cmd_date();
    else if (strcmp(work, "uptime") == 0) cmd_uptime();
    else if (strncmp(work, "touch ", 6) == 0) cmd_touch(work + 6);
    else if (strncmp(work, "rm ", 3) == 0) cmd_rm(work + 3);
    else if (strncmp(work, "cat ", 4) == 0) cmd_cat(work + 4);
    else if (strncmp(work, "edit ", 5) == 0) cmd_edit(work + 5);
    else if (strncmp(work, "mkdir ", 6) == 0) cmd_mkdir(work + 6);
    else if (strcmp(work, "cd") == 0) cmd_cd("/");
    else if (strncmp(work, "cd ", 3) == 0) cmd_cd(work + 3);
    else if (strcmp(work, "pwd") == 0) cmd_pwd();
    else if (strncmp(work, "mv ", 3) == 0) cmd_mv(work + 3);
    else if (strncmp(work, "cp ", 3) == 0) cmd_cp(work + 3);
    else if (strncmp(work, "rmdir ", 6) == 0) cmd_rmdir(work + 6);
    else if (strncmp(work, "stat ", 5) == 0) cmd_stat(work + 5);
    else if (strcmp(work, "disp") == 0) cmd_disp();
    else if (strncmp(work, "exec ", 5) == 0) {
        if (bg_job) {
            int jid = job_add(work + 5);
            set_vga_color(C_OUTPUT);
            vga_puts("["); vga_putchar('0' + jid); vga_puts("] Running\n");
            set_vga_color(C_OUTPUT);
            cmd_exec(work + 5);
            for (int i = 0; i < MAX_JOBS; i++) {
                if (job_table[i].used && job_table[i].jid == jid) {
                    set_vga_color(C_OUTPUT);
                    vga_puts("["); vga_putchar('0' + jid); vga_puts("]+ Done       ");
                    vga_puts(job_table[i].cmd); vga_putchar('\n');
                    set_vga_color(C_OUTPUT);
                    job_table[i].used = 0;
                    break;
                }
            }
        } else {
            cmd_exec(work + 5);
        }
    }
    else if (strcmp(work, "sleep") == 0) sleep_ms(1000);
    else if (strncmp(work, "sleep ", 6) == 0) {
        uint32_t ms = 0; int si = 6;
        while (work[si] >= '0' && work[si] <= '9') { ms = ms * 10 + (work[si] - '0'); si++; }
        sleep_ms(ms * 1000);
    }
    // export
    else if (strcmp(work, "export") == 0) {
        for (int i = 0; i < env_count; i++) {
            vga_puts(env_name[i]); vga_putchar('=');
            vga_puts(env_val[i]); vga_putchar('\n');
        }
    }
    else if (strncmp(work, "export ", 7) == 0) {
        const char *eq = work + 7;
        const char *sep = eq;
        while (*sep && *sep != '=') sep++;
        if (*sep == '=') {
            char vname[32]; int i; for (i = 0; eq+i < sep && i < 31; i++) vname[i] = eq[i];
            vname[i] = '\0';
            env_set(vname, sep + 1);
        } else {
            set_vga_color(C_ERROR);
            vga_puts("Uso: export VAR=valor\n");
            set_vga_color(C_OUTPUT);
        }
    }
    // alias / unalias
    else if (strcmp(work, "alias") == 0) {
        for (int i = 0; i < alias_count; i++) {
            vga_puts(alias_name[i]); vga_puts("='"); vga_puts(alias_val[i]); vga_puts("'\n");
        }
    }
    else if (strncmp(work, "alias ", 6) == 0) {
        const char *eq = work + 6;
        const char *sep = eq;
        while (*sep && *sep != '=') sep++;
        if (*sep == '=') {
            char aname[64]; int i; for (i = 0; eq+i < sep && i < 63; i++) aname[i] = eq[i];
            aname[i] = '\0';
            alias_set(aname, sep + 1);
            set_vga_color(C_SUCCESS);
            vga_puts("Alias definido: "); vga_puts(aname); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_ERROR);
            vga_puts("Uso: alias nome='comando'\n");
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "unalias") == 0) {
        set_vga_color(C_ERROR);
        vga_puts("Uso: unalias <nome>\n");
        set_vga_color(C_OUTPUT);
    }
    else if (strncmp(work, "unalias ", 8) == 0) {
        if (alias_unset(work + 8)) {
            set_vga_color(C_SUCCESS);
            vga_puts("Alias removido: "); vga_puts(work + 8); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_ERROR);
            vga_puts("Alias nao encontrado: "); vga_puts(work + 8); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    // source — execute commands from file
    else if (strncmp(work, "source ", 7) == 0) {
        const char *fn = work + 7;
        static uint8_t sbuf[4096];
        int n = fat32_read_file(fn, sbuf, 4096);
        if (n > 0) {
            char line[CMD_MAX]; int li = 0;
            for (int i = 0; i <= n; i++) {
                if (i == n || sbuf[i] == '\n') {
                    line[li] = '\0';
                    if (li > 0) execute_command(line);
                    li = 0;
                } else if (li < CMD_MAX-1) {
                    line[li++] = sbuf[i];
                }
            }
        } else {
            set_vga_color(C_ERROR);
            vga_puts("Nao encontrado: "); vga_puts(fn); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    // jobs
    else if (strcmp(work, "jobs") == 0) {
        int any = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_table[i].used) {
                set_vga_color(C_OUTPUT);
                vga_puts("["); vga_putchar('0' + job_table[i].jid);
                vga_puts("]+ Running    "); vga_puts(job_table[i].cmd);
                vga_putchar('\n'); set_vga_color(C_OUTPUT);
                any = 1;
            }
        }
        if (!any) { set_vga_color(C_OUTPUT); vga_puts("(nenhum job)\n"); set_vga_color(C_OUTPUT); }
    }
    // fg <jid> — retoma job em foreground
    else if (strncmp(work, "fg ", 3) == 0) {
        int jid = work[3] - '0';
        int idx = job_find(jid);
        if (idx < 0) {
            set_vga_color(C_ERROR); vga_puts("Job nao encontrado\n"); set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_OUTPUT);
            vga_puts("fg: "); vga_puts(job_table[idx].cmd); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "fg") == 0) {
        set_vga_color(C_ERROR); vga_puts("Uso: fg <job>\n"); set_vga_color(C_OUTPUT);
    }
    // bg <jid> — continua job em background (stub, sem scheduler)
    else if (strncmp(work, "bg ", 3) == 0) {
        int jid = work[3] - '0';
        int idx = job_find(jid);
        if (idx < 0) {
            set_vga_color(C_ERROR); vga_puts("Job nao encontrado\n"); set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_OUTPUT);
            vga_puts("bg: "); vga_puts(job_table[idx].cmd); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "bg") == 0) {
        set_vga_color(C_ERROR); vga_puts("Uso: bg <job>\n"); set_vga_color(C_OUTPUT);
    }
    // PATH search
    else if (*work != '\0') {
        const char *path = env_get("PATH");
        if (!path) path = "/BIN:/USR/BIN:/LOCAL/BIN";
        char pbuf[64]; int pi = 0; int found = 0;
        for (int i = 0; !found; i++) {
            if (path[i] == ':' || path[i] == '\0') {
                pbuf[pi] = '\0';
                if (pi > 0 && cmd_exec_in_dir(work, pbuf) == 0) found = 1;
                pi = 0;
                if (path[i] == '\0') break;
            } else {
                if (pi < 63) pbuf[pi++] = path[i];
            }
        }
        if (!found) {
            last_exit_code = 1;
            set_vga_color(C_ERROR);
            vga_puts("Comando nao encontrado: ");
            vga_puts(work);
            vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            last_exit_code = 0;
        }
    }

    // ── flush redirection buffer to file ─────────────────
    if (redir_file && redir_active) {
        redir_active = 0;
        if (redir_append) {
            // read existing, append, write back
            static uint8_t old[16384];
            int old_n = fat32_read_file(redir_file, old, 16384);
            if (old_n < 0) old_n = 0;
            int total = old_n + redir_len;
            if (total > 16384) total = 16384;
            // copy old + new into a single buffer
            static uint8_t merged[16384];
            for (int i = 0; i < old_n && i < 16384; i++) merged[i] = old[i];
            for (int i = 0; i < total - old_n; i++) merged[old_n + i] = redir_buf[i];
            fat32_write_file(redir_file, merged, total);
        } else {
            if (redir_len > 0) {
                fat32_create_file(redir_file);
                fat32_write_file(redir_file, (uint8_t*)redir_buf, redir_len);
            }
        }
    }

    // ── handle pipe ─────────────────────────────────────
    if (pipe_mode) {
        set_vga_color(C_ERROR);
        vga_puts("[pipe not fully implemented yet]\n");
        set_vga_color(C_OUTPUT);
    }
}


static void build_prompt(char *prompt, int maxlen) {
    const char *p = env_get("PS1");
    if (!p) p = "[\\w]\\$ ";
    int o = 0;
    for (int i = 0; p[i] && o < maxlen-1; i++) {
        if (p[i] == '\\' && p[i+1]) {
            i++;
            if (p[i] == '\\')      { prompt[o++] = '\\'; }
            else if (p[i] == 'u')  { const char *u = "root"; for (int j = 0; u[j] && o < maxlen-1; j++) prompt[o++] = u[j]; }
            else if (p[i] == 'h')  { const char *h = "ovsb"; for (int j = 0; h[j] && o < maxlen-1; j++) prompt[o++] = h[j]; }
            else if (p[i] == 's')  { const char *sh = "MkM"; for (int j = 0; sh[j] && o < maxlen-1; j++) prompt[o++] = sh[j]; }
            else if (p[i] == '$')  { prompt[o++] = '#'; }
            else if (p[i] == 'w')  {
                char path[256];
                fat32_get_cwd_name(path, 256);
                for (int j = 0; path[j] && o < maxlen-1; j++) prompt[o++] = path[j];
            }
            else if (p[i] == 'W')  {
                char path[256];
                fat32_get_cwd_name(path, 256);
                int last = 0;
                for (int j = 0; path[j]; j++) if (path[j] == '/') last = j;
                const char *base = (last > 0) ? path + last + 1 : path;
                for (int j = 0; base[j] && o < maxlen-1; j++) prompt[o++] = base[j];
            }
        } else {
            prompt[o++] = p[i];
        }
    }
    prompt[o] = '\0';
}

void shell_loop() {
    static char cmd[CMD_MAX];
    int pos = 0, len = 0;
    static int tab_count = 0;
    static char prompt[256];

    build_prompt(prompt, 256);
    set_vga_color(C_PROMPT);
    vga_puts(prompt);
    set_vga_color(C_COMMAND);

    while (1) {
        int k = read_key();

        // ── Enter ──────────────────────────────────────────
        if (k == '\n') {
            vga_putchar('\n');
            cmd[len] = '\0';
            if (len > 0) {
                int hi = hist_count % HIST_MAX;
                int j;
                for (j = 0; j < len && j < CMD_MAX-1; j++) history[hi][j] = cmd[j];
                history[hi][j] = '\0';
                hist_count++;
                execute_command(cmd);
                env_set("?", last_exit_code ? "1" : "0");
            }
            pos = len = 0;
            build_prompt(prompt, 256);
            set_vga_color(C_PROMPT);
            vga_puts(prompt);
            set_vga_color(C_COMMAND);
            continue;
        }

        // ── Tab: autocomplete ─────────────────────────────────
        if (k == '\t') {
            tab_count++;
            // Find word start (beginning or after space)
            int ws = pos;
            while (ws > 0 && cmd[ws-1] != ' ') ws--;
            int plen = pos - ws;
            char prefix[64]; int i;
            for (i = 0; i < plen && i < 63; i++) prefix[i] = cmd[ws + i];
            prefix[i] = '\0';
            // Check if first token on line
            int is_first = 1;
            for (int j = 0; j < ws; j++) if (cmd[j] != ' ') { is_first = 0; break; }
            fat32_dirent_t matches[64];
            int nm = 0;
            if (is_first) {
                static const char *bi[] = {"help","clear","echo","about","shutdown",
                    "reboot","source","ls","date","uptime","touch","rm","cat","edit","mkdir","cd",
                    "pwd","mv","cp","rmdir","stat","disp","exec","sleep",NULL};
                for (int j = 0; bi[j] && nm < 64; j++)
                    if (strncmp(bi[j], prefix, plen) == 0) {
                        int kk; for (kk = 0; bi[j][kk] && kk < 63; kk++)
                            matches[nm].name[kk] = bi[j][kk];
                        matches[nm].name[kk] = '\0';
                        matches[nm].attr = 0; nm++;
                    }
                char saved[256];
                fat32_get_cwd_name(saved, 256);
                // Scan PATH directories from $PATH env var
                const char *path = env_get("PATH");
                if (!path) path = "/BIN:/USR/BIN:/LOCAL/BIN";
                char pbuf[64]; int pi = 0;
                for (int pi2 = 0; nm < 64; pi2++) {
                    if (path[pi2] == ':' || path[pi2] == '\0') {
                        pbuf[pi] = '\0';
                        if (pi > 0) {
                            if (fat32_change_dir(pbuf) == 0) {
                                int n2 = fat32_match_prefix(prefix, fat32_get_cwd(),
                                                             matches + nm, 64 - nm);
                                nm += n2;
                            }
                        }
                        pi = 0;
                        if (path[pi2] == '\0') break;
                    } else {
                        if (pi < 63) pbuf[pi++] = path[pi2];
                    }
                }
                fat32_change_dir(saved[0] ? saved : "/");
            } else {
                uint32_t cwd = fat32_get_cwd();
                nm = fat32_match_prefix(prefix, cwd, matches, 64);
            }
            if (nm == 0) { /* no matches, ignore */ }
            else if (nm == 1) {
                // Complete with the single match (skip prefix part)
                char *s = matches[0].name;
                int slen = 0; while (s[slen]) slen++;
                // How many chars to insert?
                int extra = slen - plen; // may be negative if prefix longer
                // Add trailing / for directories
                int add_slash = (matches[0].attr & 0x10) ? 1 : (is_first ? 1 : 0);
                // Shift rest of cmd to make room
                if (extra + add_slash > 0) {
                    int shift = extra + add_slash;
                    for (int j = len; j >= pos; j--) cmd[j + shift] = cmd[j];
                } else if (extra + add_slash < 0) {
                    int shift = -(extra + add_slash);
                    for (int j = pos; j <= len; j++) cmd[j - shift] = cmd[j];
                }
                // Write the completion (skip already-typed prefix)
                for (int j = 0; j < slen - plen; j++)
                    cmd[ws + plen + j] = s[plen + j];
                // Add trailing space (or / for dirs)
                int trail_idx = slen - plen;
                if (matches[0].attr & 0x10) {
                    cmd[ws + plen + trail_idx] = '/';
                    trail_idx++;
                } else if (is_first) {
                    cmd[ws + plen + trail_idx] = ' ';
                    trail_idx++;
                }
                len += extra + trail_idx;
                pos = ws + slen + (trail_idx > 0 ? trail_idx : 0);
                goto redraw;
            } else {
                // Multiple matches: find longest common prefix
                char common[64];
                int ci = 0;
                while (1) {
                    char ck = matches[0].name[ci];
                    if (!ck) break;
                    int ok = 1;
                    for (int j = 1; j < nm; j++)
                        if (matches[j].name[ci] != ck) { ok = 0; break; }
                    if (!ok) break;
                    common[ci++] = ck;
                }
                common[ci] = '\0';
                // If common is longer than prefix, complete with common
                if (ci > plen) {
                    int extra = ci - plen;
                    for (int j = len; j >= pos; j--) cmd[j + extra] = cmd[j];
                    for (int j = 0; j < extra; j++)
                        cmd[ws + plen + j] = common[plen + j];
                    len += extra;
                    pos = ws + ci;
                    goto redraw;
                }
                // On double-tab, list matches
                if (tab_count >= 2) {
                    vga_putchar('\n');
                    for (int j = 0; j < nm; j++) {
                        if (matches[j].attr & 0x10) set_vga_color(C_DIR);
                        else set_vga_color(C_FILE);
                        vga_puts(matches[j].name);
                        if (matches[j].attr & 0x10) vga_putchar('/');
                        vga_putchar(' ');
                    }
                    set_vga_color(C_OUTPUT);
                    vga_putchar('\n');
                    set_vga_color(C_PROMPT);
                    vga_puts(prompt);
                    set_vga_color(C_COMMAND);
                    vga_puts(cmd);
                    // redraw resets cursor position based on pos
                    goto redraw;
                }
            }
            continue;
        }

        // ── Any non-TAB resets tab counter ──────────────────
        tab_count = 0;

        // ── Ctrl keys ──────────────────────────────────────
        if (k == 1)  { pos = 0; goto redraw; }              // ^A = home
        if (k == 5)  { pos = len; goto redraw; }             // ^E = end
        if (k == 11) {                                      // ^K = kill to end
            len = pos; cmd[len] = 0; goto redraw;
        }
        if (k == 12) { vga_clear(); vga_puts(prompt); vga_puts(cmd); goto redraw; } // ^L
        if (k == 21) {                                      // ^U = kill to start
            int diff = pos; len -= diff;
            for (int i = 0; i < len; i++) cmd[i] = cmd[i + diff];
            pos = 0; goto redraw;
        }
        if (k == 23) {                                      // ^W = kill word backward
            if (pos > 0) {
                int end = pos - 1;
                while (end > 0 && cmd[end-1] == ' ') end--;
                while (end > 0 && cmd[end-1] != ' ') end--;
                int diff = pos - end;
                for (int i = end; i + diff <= len; i++) cmd[i] = cmd[i + diff];
                len -= diff; pos = end; goto redraw;
            }
        }

        // ── History navigation ─────────────────────────────
        if (k == K_UP || k == K_DOWN) {
            if (hist_count == 0) continue;
            int hi;
            // pos tracks position relative to current entry
            static int browse_idx = -1; // -1 = not browsing
            if (k == K_UP) {
                if (browse_idx < 0) {
                    // save current line as temporary entry
                    cmd[len] = 0;
                    browse_idx = hist_count % HIST_MAX;
                    int j;
                    for (j = 0; j < len && j < CMD_MAX-1; j++) history[browse_idx][j] = cmd[j];
                    history[browse_idx][j] = '\0';
                    browse_idx = (hist_count > HIST_MAX) ? HIST_MAX - 1 : hist_count - 1;
                } else if (browse_idx > 0) {
                    browse_idx--;
                }
                hi = browse_idx;
            } else {
                if (browse_idx < 0) continue;
                int max_browse = (hist_count > HIST_MAX) ? HIST_MAX - 1 : hist_count - 1;
                if (browse_idx < max_browse) {
                    browse_idx++;
                    hi = browse_idx;
                } else {
                    // back to original line
                    browse_idx = -1;
                    pos = len = 0;
                    goto redraw;
                }
            }
            if (browse_idx >= 0) {
                // copy history entry to cmd
                const char *src = history[browse_idx >= HIST_MAX ? browse_idx % HIST_MAX : browse_idx];
                int j;
                for (j = 0; src[j] && j < CMD_MAX-1; j++) cmd[j] = src[j];
                len = j; cmd[len] = 0; pos = len;
                goto redraw;
            }
            continue;
        }

        // ── Arrow keys / navigation ────────────────────────
        if (k == K_LEFT)  { if (pos > 0) pos--; goto redraw; }
        if (k == K_RIGHT) { if (pos < len) pos++; goto redraw; }
        if (k == K_HOME)  { pos = 0; goto redraw; }
        if (k == K_END)   { pos = len; goto redraw; }

        // ── Delete ─────────────────────────────────────────
        if (k == K_DEL) {
            if (pos < len) {
                for (int i = pos; i < len - 1; i++) cmd[i] = cmd[i + 1];
                len--;
                if (pos < len) goto redraw; else { vga_puts("\x1b[P"); }
            }
            continue;
        }

        // ── Backspace ──────────────────────────────────────
        if (k == '\b' || k == 127) {
            if (pos > 0) {
                pos--; len--;
                for (int i = pos; i < len; i++) cmd[i] = cmd[i + 1];
                if (pos < len) goto redraw; else { vga_putchar('\b'); }
            }
            continue;
        }

        // ── Printable characters ───────────────────────────
        if (k >= 32 && k <= 126) {
            if (len >= CMD_MAX - 1) continue;
            if (pos == len) {
                // Append at end: fast path, no redraw
                cmd[pos] = k;
                vga_putchar(k);
                pos++; len++;
            } else {
                // Insert in middle: need redraw
                for (int i = len; i > pos; i--) cmd[i] = cmd[i - 1];
                cmd[pos] = k;
                pos++; len++; goto redraw;
            }
            continue;
        }

        continue;

    redraw:
        // ── redraw command line ────────────────────────────
        vga_putchar('\r');
        vga_puts("\x1b[K");
        build_prompt(prompt, 256);
        set_vga_color(C_PROMPT);
        vga_puts(prompt);
        set_vga_color(C_COMMAND);
        if (len > 0) vga_puts(cmd);
        int nback = len - pos;
        while (nback > 0) { vga_putchar('\b'); nback--; }
    }
}

void kmain(void) {
    idt_init();
    pic_init();
    idt_set_syscall();
    idt_set_irq1();
    keyboard_init();
    pit_init();         // Program PIT to 100Hz
    memory_init();
    tss_init();
    __asm__ volatile ("sti");
    smc_init();
    nvram_init();
    serial_init();
    ata_init();
    env_init();

    vga_clear();
    set_vga_color(C_HEADER);
    vga_puts("OvsbMkM Terminal v4.0\n");
    
    if (fat32_init() == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("FAT32 OK\n");
    } else {
        set_vga_color(C_ERROR);
        vga_puts("Erro FAT32\n");
    }
    set_vga_color(C_OUTPUT);
    vga_puts("Digite 'help' para comandos.\n");
    shell_loop();
}
