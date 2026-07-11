#include "idt.h"
#include "memory.h"
#include "../drivers/ata.h"
#include "fat32.h"
#include "../commands/shell_cmds.h"

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read(void);
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

void vga_putchar(char c) {
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
        vga[cy * VGA_WIDTH + cx] = (COLOR << 8) | c; cx++;
        serial_putc(c);
    }
    if (cx >= VGA_WIDTH) { cx = 0; cy++; }
    if (cy >= VGA_HEIGHT) {
        for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) vga[i] = vga[i + VGA_WIDTH];
        for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = (COLOR << 8) | ' ';
        cy = VGA_HEIGHT - 1;
    }
    unsigned short pos = cy * VGA_WIDTH + cx;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}
void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = (COLOR << 8) | ' ';
    cx = cy = 0;
}

void debug_puts(const char *s) {
    vga_puts(s);
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

void execute_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) cmd_help();
    else if (strcmp(cmd, "clear") == 0) cmd_clear();
    else if (strncmp(cmd, "echo ", 5) == 0) cmd_echo(cmd + 5);
    else if (strcmp(cmd, "about") == 0) cmd_about();
    else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();
    else if (strcmp(cmd, "ls") == 0) cmd_ls();
    else if (strncmp(cmd, "touch ", 6) == 0) cmd_touch(cmd + 6);
    else if (strncmp(cmd, "rm ", 3) == 0) cmd_rm(cmd + 3);
    else if (strncmp(cmd, "cat ", 4) == 0) cmd_cat(cmd + 4);
    else if (strncmp(cmd, "edit ", 5) == 0) cmd_edit(cmd + 5);
    else if (strncmp(cmd, "mkdir ", 6) == 0) cmd_mkdir(cmd + 6);
    else if (strcmp(cmd, "cd") == 0) cmd_cd("/");
    else if (strncmp(cmd, "cd ", 3) == 0) cmd_cd(cmd + 3);
    else if (strcmp(cmd, "pwd") == 0) cmd_pwd();
    else if (strncmp(cmd, "exec ", 5) == 0) cmd_exec(cmd + 5);
    else if (*cmd != '\0') {
        vga_puts("Comando nao reconhecido: ");
        vga_puts(cmd);
        vga_putchar('\n');
    }
}

void shell_loop() {
    static char cmd[256];
    int pos = 0;

    vga_puts("MkM> ");
    while (1) {
        char c = keyboard_read();
        if (c == '\n') {
            cmd[pos] = '\0';
            vga_putchar('\n');
            execute_command(cmd);
            pos = 0;
            vga_puts("MkM> ");
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                vga_putchar('\b');
            }
        } else if (c >= ' ' && c <= '~' && pos < 255) {
            cmd[pos++] = c;
            vga_putchar(c);
        }
    }
}

void kmain(void) {
    idt_init();
    pic_init();
    idt_set_syscall();
    idt_set_irq1();
    keyboard_init();
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
