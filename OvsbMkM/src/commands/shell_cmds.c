#include "shell_cmds.h"
#include "../kernel/idt.h"
#include "../kernel/mach_o.h"
#include "../kernel/kernel.h"
#include "../fs/fat32.h"

extern void vga_puts(const char *s);
extern void vga_putchar(char c);
extern void vga_clear(void);
extern char keyboard_read(void);
extern volatile uint64_t timer_ticks;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void sync_fs(void) {
    fat32_sync();
    vga_puts("FS OK\n");
}

static void print_err(int err, const char *name) {
    if (err == 0) return;
    set_vga_color(C_ERROR);
    if (err == FAT_ERR_NOTFOUND) { vga_puts("Nao encontrado: "); vga_puts(name); vga_putchar('\n'); }
    else if (err == FAT_ERR_EXISTS) { vga_puts("Ja existe: "); vga_puts(name); vga_putchar('\n'); }
    else if (err == FAT_ERR_NOTDIR) vga_puts("Nao e diretorio\n");
    else if (err == FAT_ERR_NOSPACE) vga_puts("Disco cheio\n");
    else if (err == FAT_ERR_IO) vga_puts("Erro de E/S\n");
    else { vga_puts("Erro "); vga_putchar('0' + (-err)); vga_putchar('\n'); }
    set_vga_color(C_OUTPUT);
}

void cmd_help(void) {
    set_vga_color(C_HEADER);
    vga_puts("Comandos disponiveis:\n");
    set_vga_color(C_OUTPUT);
    vga_puts("help  clear  echo  about  shutdown\n");
    vga_puts("ls    touch  rm    cat    edit\n");
    vga_puts("mkdir cd     pwd   exec\n");
    vga_puts("mv    cp     rmdir stat  disp\n");
}

void cmd_clear(void) {
    vga_clear();
}

void cmd_echo(const char *args) {
    vga_puts(args);
    vga_putchar('\n');
}

void cmd_about(void) {
    set_vga_color(C_HEADER);
    vga_puts("OvsbMkM - Micro Kernel\n");
    set_vga_color(C_OUTPUT);
}

void cmd_shutdown(void) {
    sync_fs();
    set_vga_color(C_ERROR);
    vga_puts("Desligando...\n");
    __asm__ volatile ("cli; hlt");
}

void cmd_reboot(void) {
    sync_fs();
    set_vga_color(C_ERROR);
    vga_puts("Reiniciando...\n");
    uint8_t g;
    do { g = inb(0x64); } while (g & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile ("cli; hlt");
}

void cmd_ls(void) {
    int count = fat32_list_dir();
    if (count == 0) {
        set_vga_color(C_OUTPUT);
        vga_puts("(vazio)\n");
    }
}

void cmd_touch(const char *name) {
    int r = fat32_create_file(name);
    if (r == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("Criado: ");
        vga_puts(name);
        vga_putchar('\n');
        set_vga_color(C_OUTPUT);
    } else {
        print_err(r, name);
    }
}

void cmd_rm(const char *name) {
    if (fat32_delete_file(name) == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("Removido: ");
        vga_puts(name);
        vga_putchar('\n');
        set_vga_color(C_OUTPUT);
    } else {
        set_vga_color(C_ERROR);
        vga_puts("Erro\n");
        set_vga_color(C_OUTPUT);
    }
}

void cmd_cat(const char *name) {
    static uint8_t buffer[4096];
    int bytes = fat32_read_file(name, buffer, 4096);
    if (bytes < 0) {
        set_vga_color(C_ERROR);
        vga_puts("Erro\n");
        set_vga_color(C_OUTPUT);
    } else if (bytes == 0) {
        // empty file, no output
    } else {
        for (int i = 0; i < bytes; i++) vga_putchar(buffer[i]);
    }
}

void cmd_edit(const char *name) {
    set_vga_color(C_HEADER);
    vga_puts("Editor - ESC salva / TAB descarta:\n");
    set_vga_color(C_OUTPUT);
    static char buf[4096];
    int pos = 0;
    int col = 0;
    while (1) {
        char c = keyboard_read();
        if (c == 27) break;
        if (c == '\t') { set_vga_color(C_ERROR); vga_puts("\nCancelado\n"); set_vga_color(C_OUTPUT); return; }
        if (c == '\b') {
            if (pos > 0) { pos--; vga_putchar('\b'); if (col > 0) col--; }
            continue;
        }
        if (c == '\n') { buf[pos++] = '\n'; vga_putchar('\n'); col = 0; continue; }
        if (c >= ' ' && c <= '~' && pos < 4095) {
            buf[pos++] = c; vga_putchar(c); col++;
            if (col >= 80) { vga_putchar('\n'); col = 0; }
        }
    }
    vga_puts("\n");
    set_vga_color(C_HEADER);
    vga_puts("Salvando...\n");
    set_vga_color(C_OUTPUT);
    if (fat32_write_file(name, (uint8_t*)buf, pos) > 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("OK\n");
    } else {
        set_vga_color(C_ERROR);
        vga_puts("Erro\n");
    }
    set_vga_color(C_OUTPUT);
}

void cmd_mkdir(const char *name) {
    int r = fat32_mkdir(name);
    if (r == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("Criado: ");
        vga_puts(name);
        vga_putchar('/');
        vga_putchar('\n');
        set_vga_color(C_OUTPUT);
    } else {
        print_err(r, name);
    }
}

void cmd_cd(const char *name) {
    if (name[0] == '\0') name = "/";
    int r = fat32_change_dir(name);
    if (r != 0) print_err(r, name);
}

void cmd_pwd(void) {
    char path[256];
    fat32_get_cwd_name(path, 256);
    vga_puts(path);
    vga_putchar('\n');
}

#define BUF_SIZE 65536

int cmd_exec_in_dir(const char *name, const char *dir) {
    if (!name || !*name) return -1;
    if (fat32_change_dir(dir) != 0) return -1;
    static uint8_t buf[BUF_SIZE];
    int bytes = fat32_read_file(name, buf, BUF_SIZE);
    if (bytes <= 0) { fat32_change_dir("/"); return -1; }
    void *entry = mach_o_load(buf, bytes);
    if (!entry) { fat32_change_dir("/"); return -1; }
    ((void (*)(void))entry)();
    fat32_change_dir("/");
    return 0;
}

void cmd_exec(const char *name) {
    static uint8_t buf[BUF_SIZE];
    void *entry = 0;
    int bytes;

    bytes = fat32_read_file(name, buf, BUF_SIZE);
    if (bytes <= 0) {
        set_vga_color(C_ERROR);
        vga_puts("Erro: arquivo nao encontrado\n");
        set_vga_color(C_OUTPUT);
        return;
    }

    entry = mach_o_load(buf, bytes);
    if (!entry) {
        set_vga_color(C_ERROR);
        vga_puts("Erro: formato invalido\n");
        set_vga_color(C_OUTPUT);
        return;
    }

    set_vga_color(C_HEADER);
    vga_puts("---\n");
    set_vga_color(C_OUTPUT);
    ((void (*)(void))entry)();
    set_vga_color(C_HEADER);
    vga_puts("\n---\n");
    set_vga_color(C_OUTPUT);
}

void cmd_mv(const char *args) {
    char src[64], dst[64];
    int i = 0;
    while (*args == ' ') args++;
    while (*args && *args != ' ' && i < 63) src[i++] = *args++;
    src[i] = '\0';
    while (*args == ' ') args++;
    i = 0;
    while (*args && *args != ' ' && i < 63) dst[i++] = *args++;
    dst[i] = '\0';
    if (src[0] == '\0' || dst[0] == '\0') {
        vga_puts("Uso: mv <origem> <destino>\n");
        return;
    }
    int r = fat32_rename(src, dst);
    if (r == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("Renomeado: "); vga_puts(src); vga_puts(" -> "); vga_puts(dst); vga_putchar('\n');
        set_vga_color(C_OUTPUT);
    }
    else print_err(r, src);
}

void cmd_cp(const char *args) {
    char src[64], dst[64];
    int i = 0;
    while (*args == ' ') args++;
    while (*args && *args != ' ' && i < 63) src[i++] = *args++;
    src[i] = '\0';
    while (*args == ' ') args++;
    i = 0;
    while (*args && *args != ' ' && i < 63) dst[i++] = *args++;
    dst[i] = '\0';
    if (src[0] == '\0' || dst[0] == '\0') {
        vga_puts("Uso: cp <origem> <destino>\n");
        return;
    }
    static uint8_t buf[4096];
    int bytes = fat32_read_file(src, buf, 4096);
    if (bytes < 0) { print_err(FAT_ERR_NOTFOUND, src); return; }
    if (fat32_create_file(dst) != 0 && fat32_write_file(dst, buf, bytes) <= 0) {
        set_vga_color(C_ERROR);
        vga_puts("Erro ao copiar\n");
        set_vga_color(C_OUTPUT);
        return;
    }
    fat32_write_file(dst, buf, bytes);
    set_vga_color(C_SUCCESS);
    vga_puts("Copiado: "); vga_puts(src); vga_puts(" -> "); vga_puts(dst); vga_putchar('\n');
    set_vga_color(C_OUTPUT);
}

void cmd_rmdir(const char *name) {
    if (name[0] == '\0') { vga_puts("Uso: rmdir <dir>\n"); return; }
    int r = fat32_rmdir(name);
    if (r == 0) {
        set_vga_color(C_SUCCESS);
        vga_puts("Removido: "); vga_puts(name); vga_puts("/\n");
        set_vga_color(C_OUTPUT);
    }
    else print_err(r, name);
}

void cmd_disp(void) {
    vga_puts("Iniciando modo grafico (ESC para sair)...\n");
    extern void vga_gfx_init(void);
    extern void vga_gfx_restore_text(void);
    extern void vga_gfx_clear(uint8_t);
    extern void vga_gfx_fillrect(int,int,int,int,uint8_t);
    extern void disp_init(void);
    extern void disp_render(void);
    extern void disp_update_mouse(int,int);
    extern char keyboard_read(void);
    vga_gfx_init();
    disp_init();
    disp_render();
    int running = 1;
    while (running) {
        char c = keyboard_read();
        switch (c) {
            case 27: running = 0; break;
            case 'w': disp_update_mouse(0, -5); break;
            case 's': disp_update_mouse(0, 5); break;
            case 'a': disp_update_mouse(-5, 0); break;
            case 'd': disp_update_mouse(5, 0); break;
        }
        disp_render();
    }
    vga_gfx_restore_text();
    vga_clear();
    vga_puts("Modo texto restaurado.\n");
}

void cmd_date(void) {
    rtc_time t;
    rtc_read(&t);
    // Format: YYYY-MM-DD HH:MM:SS
    char n[4]; int p;
    // year
    p = 0; int v = t.yr; do { n[p++] = '0' + (v % 10); v /= 10; } while (v);
    while (p > 0) vga_putchar(n[--p]);
    vga_putchar('-');
    // month
    p = 0; v = t.mo; if (v == 0) { vga_putchar('0'); } else { do { n[p++] = '0' + (v % 10); v /= 10; } while (v); while (p > 0) vga_putchar(n[--p]); }
    vga_putchar('-');
    // day
    p = 0; v = t.dy; if (v == 0) { vga_putchar('0'); } else { do { n[p++] = '0' + (v % 10); v /= 10; } while (v); while (p > 0) vga_putchar(n[--p]); }
    vga_putchar(' ');
    // hour
    p = 0; v = t.h; do { n[p++] = '0' + (v % 10); v /= 10; } while (v);
    if (p < 2) vga_putchar('0');
    while (p > 0) vga_putchar(n[--p]);
    vga_putchar(':');
    // minute
    p = 0; v = t.m; do { n[p++] = '0' + (v % 10); v /= 10; } while (v);
    if (p < 2) vga_putchar('0');
    while (p > 0) vga_putchar(n[--p]);
    vga_putchar(':');
    // second
    p = 0; v = t.s; do { n[p++] = '0' + (v % 10); v /= 10; } while (v);
    if (p < 2) vga_putchar('0');
    while (p > 0) vga_putchar(n[--p]);
    vga_putchar('\n');
}

void cmd_uptime(void) {
    uint64_t sec = timer_ticks / 100; // 100 ticks/sec
    uint64_t days = sec / 86400; sec %= 86400;
    uint64_t hours = sec / 3600; sec %= 3600;
    uint64_t mins = sec / 60; sec %= 60;
    // use same pattern as date
    char n[12]; int p;
    vga_puts("up ");
    if (days > 0) { p = 0; uint64_t v = days; do { n[p++] = '0' + (v % 10); v /= 10; } while (v); while (p > 0) vga_putchar(n[--p]); vga_putchar('d'); }
    p = 0; uint64_t v = hours; if (v == 0) n[p++] = '0'; else do { n[p++] = '0' + (v % 10); v /= 10; } while (v); while (p > 0) vga_putchar(n[--p]);
    vga_putchar(':');
    p = 0; v = mins; if (v == 0) n[p++] = '0'; else do { n[p++] = '0' + (v % 10); v /= 10; } while (v); if (p < 2) vga_putchar('0');
    while (p > 0) vga_putchar(n[--p]);
    vga_putchar('\n');
}

void cmd_stat(const char *name) {
    if (name[0] == '\0') { vga_puts("Uso: stat <arquivo>\n"); return; }
    uint32_t size;
    uint8_t attr;
    uint16_t mtime=0, mdate=0;
    int r = fat32_stat(name, &size, &attr, &mtime, &mdate);
    if (r != 0) { print_err(r, name); return; }
    vga_puts(name);
    vga_puts("  tam=");
    char n[12]; int p = 0;
    uint32_t s = size;
    do { n[p++] = '0' + (s % 10); s /= 10; } while (s);
    while (p > 0) vga_putchar(n[--p]);
    vga_puts("  attr=");
    if (attr & 0x10) vga_puts("DIR");
    else if (attr & 0x20) vga_puts("ARC");
    else vga_puts("---");
    if (mtime || mdate) {
        // FAT32 time: hhhhhhmmmmmmsssss → hours(15-11) min(10-5) sec/2(4-0)
        int h = (mtime >> 11) & 0x1F;
        int m = (mtime >> 5) & 0x3F;
        int s = (mtime & 0x1F) * 2;
        // FAT32 date: yyyyyyymmmmddddd → year-1980(15-9) month(8-5) day(4-0)
        int y = ((mdate >> 9) & 0x7F) + 1980;
        int mo = (mdate >> 5) & 0x0F;
        int d = mdate & 0x1F;
        vga_puts("  ");
        // date YYYY-MM-DD
        char db[11]; int di = 0;
        int n = y; if (n >= 1000) { db[di++] = '0' + n/1000; n %= 1000; }
        if (n >= 100) { db[di++] = '0' + n/100; n %= 100; }
        if (n >= 10) { db[di++] = '0' + n/10; }
        db[di++] = '0' + n;
        db[di++] = '-';
        db[di++] = '0' + mo/10; db[di++] = '0' + mo%10;
        db[di++] = '-';
        db[di++] = '0' + d/10; db[di++] = '0' + d%10;
        db[di++] = ' ';
        db[di++] = '0' + h/10; db[di++] = '0' + h%10;
        db[di++] = ':';
        db[di++] = '0' + m/10; db[di++] = '0' + m%10;
        db[di++] = ':';
        db[di++] = '0' + s/10; db[di++] = '0' + s%10;
        for (int i = 0; i < di; i++) vga_putchar(db[i]);
    }
    vga_putchar('\n');
}