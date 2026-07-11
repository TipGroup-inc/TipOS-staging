#include "shell_cmds.h"
#include "../kernel/idt.h"
#include "../kernel/mach_o.h"
#include "../kernel/kernel.h"
#include "../fs/fat32.h"

extern void vga_puts(const char *s);
extern void vga_putchar(char c);
extern void vga_clear(void);
extern char keyboard_read(void);

static void print_err(int err, const char *name) {
    if (err == 0) return;
    if (err == FAT_ERR_NOTFOUND) { vga_puts("Nao encontrado: "); vga_puts(name); vga_putchar('\n'); }
    else if (err == FAT_ERR_EXISTS) { vga_puts("Ja existe: "); vga_puts(name); vga_putchar('\n'); }
    else if (err == FAT_ERR_NOTDIR) vga_puts("Nao e diretorio\n");
    else if (err == FAT_ERR_NOSPACE) vga_puts("Disco cheio\n");
    else if (err == FAT_ERR_IO) vga_puts("Erro de E/S\n");
    else { vga_puts("Erro "); vga_putchar('0' + (-err)); vga_putchar('\n'); }
}

void cmd_help(void) {
    vga_puts("help  clear  echo  about  shutdown\n");
    vga_puts("ls    touch  rm    cat    edit\n");
    vga_puts("mkdir cd     pwd   exec\n");
}

void cmd_clear(void) {
    vga_clear();
}

void cmd_echo(const char *args) {
    vga_puts(args);
    vga_putchar('\n');
}

void cmd_about(void) {
    vga_puts("OvsbMkM - Micro Kernel\n");
}

void cmd_shutdown(void) {
    vga_puts("Desligando...\n");
    __asm__ volatile ("cli; hlt");
}

void cmd_ls(void) {
    int count = fat32_list_dir();
    if (count == 0) vga_puts("(vazio)\n");
}

void cmd_touch(const char *name) {
    int r = fat32_create_file(name);
    if (r == 0) {
        vga_puts("Criado: ");
        vga_puts(name);
        vga_putchar('\n');
    } else {
        print_err(r, name);
    }
}

void cmd_rm(const char *name) {
    if (fat32_delete_file(name) == 0) {
        vga_puts("Removido: ");
        vga_puts(name);
        vga_putchar('\n');
    } else {
        vga_puts("Erro\n");
    }
}

void cmd_cat(const char *name) {
    static uint8_t buffer[4096];
    int bytes = fat32_read_file(name, buffer, 4096);
    if (bytes < 0) {
        vga_puts("Erro\n");
    } else if (bytes == 0) {
        // empty file, no output
    } else {
        for (int i = 0; i < bytes; i++) vga_putchar(buffer[i]);
    }
}

void cmd_edit(const char *name) {
    vga_puts("Editor - ESC salva / TAB descarta:\n");
    static char buf[4096];
    int pos = 0;
    int col = 0;
    while (1) {
        char c = keyboard_read();
        if (c == 27) break;
        if (c == '\t') { vga_puts("\nCancelado\n"); return; }
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
    vga_puts("\nSalvando...\n");
    if (fat32_write_file(name, (uint8_t*)buf, pos) > 0) vga_puts("OK\n");
    else vga_puts("Erro\n");
}

void cmd_mkdir(const char *name) {
    int r = fat32_mkdir(name);
    if (r == 0) {
        vga_puts("Criado: ");
        vga_puts(name);
        vga_putchar('/');
        vga_putchar('\n');
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

void cmd_exec(const char *name) {
    static uint8_t buf[4096];
    void *entry = 0;
    int bytes;

    bytes = fat32_read_file(name, buf, 4096);
    if (bytes <= 0) {
        vga_puts("Erro: arquivo nao encontrado\n");
        return;
    }

    entry = mach_o_load(buf, bytes);
    if (!entry) {
        vga_puts("Erro: formato invalido\n");
        return;
    }

    vga_puts("---\n");
    ((void (*)(void))entry)();
    vga_puts("\n---\n");
}