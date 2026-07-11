#include "idt.h"
#include "memory.h"
#include "../drivers/ata.h"
#include "fat32.h"
#include "../commands/shell_cmds.h"
#include "kernel.h"

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read(void);
int keyboard_avail(void);
void pic_init(void);
void smc_init(void);
void nvram_init(void);

#define VGA_ADDR  0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define COLOR (0x0A)

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
        vga[i] = (COLOR << 8) | ' ';
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
        vga[cy * VGA_WIDTH + cx] = (COLOR << 8) | c; cx++;
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
            for (int i = cx; i < VGA_WIDTH; i++) vga[cy * VGA_WIDTH + i] = (COLOR << 8) | ' ';
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
        if (cx > 0) { cx--; vga[cy * VGA_WIDTH + cx] = (COLOR << 8) | ' '; }
        serial_putc('\b');
    } else if (c == '\r') {
        cx = 0;
        serial_putc('\r');
    } else {
        unsigned short attr = (esc_rev ? 0x70 : COLOR) << 8;
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
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = (COLOR << 8) | ' ';
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

void execute_command(const char *cmd) {
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

    // ── handle < input redirection ───────────────────────
    if (redir_in) {
        static uint8_t ibuf[512];
        int n = fat32_read_file(in_file, ibuf, 512);
        if (n > 0 && n <= 512) {
            // feed file content as command to execute
            // simple: just cat the file
            for (int i = 0; i < n; i++) vga_putchar(ibuf[i]);
        } else {
            vga_puts("Erro: ");
            vga_puts(in_file);
            vga_puts(" nao encontrado\n");
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
    else if (strncmp(work, "exec ", 5) == 0) cmd_exec(work + 5);
    else if (strcmp(work, "sleep") == 0) sleep_ms(1000);
    else if (strncmp(work, "sleep ", 6) == 0) {
        uint32_t ms = 0; int si = 6;
        while (work[si] >= '0' && work[si] <= '9') { ms = ms * 10 + (work[si] - '0'); si++; }
        sleep_ms(ms * 1000); // sleep N seconds (convert to ms)
    }
    // PATH search
    else if (*work != '\0') {
        if (cmd_exec_in_dir(work, "/BIN") != 0 &&
            cmd_exec_in_dir(work, "/USR/BIN") != 0 &&
            cmd_exec_in_dir(work, "/LOCAL/BIN") != 0) {
            vga_puts("Comando nao encontrado: ");
            vga_puts(work);
            vga_putchar('\n');
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
        vga_puts("[pipe not fully implemented yet]\n");
    }
}

void shell_loop() {
    static char cmd[CMD_MAX];
    int pos = 0, len = 0;

    #define PROMPT "MkM> "
    vga_puts(PROMPT);

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
            }
            pos = len = 0;
            vga_puts(PROMPT);
            continue;
        }

        // ── Tab: autocomplete (simple) ─────────────────────
        if (k == '\t') {
            // stubs for now
            continue;
        }

        // ── Ctrl keys ──────────────────────────────────────
        if (k == 1)  { pos = 0; goto redraw; }              // ^A = home
        if (k == 5)  { pos = len; goto redraw; }             // ^E = end
        if (k == 11) {                                      // ^K = kill to end
            len = pos; cmd[len] = 0; goto redraw;
        }
        if (k == 12) { vga_clear(); vga_puts(PROMPT); vga_puts(cmd); goto redraw; } // ^L
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
        vga_puts(PROMPT);
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
    __asm__ volatile ("sti");
    smc_init();
    nvram_init();
    serial_init();
    ata_init();

    vga_clear();
    vga_puts("OvsbMkM Terminal v4.0\n");
    
    if (fat32_init() == 0) {
        vga_puts("FAT32 OK\n");
    } else {
        vga_puts("Erro FAT32\n");
    }
    
    vga_puts("Digite 'help' para comandos.\n");
    shell_loop();
}
