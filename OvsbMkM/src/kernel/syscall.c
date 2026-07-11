#include "idt.h"
#include "memory.h"
#include <stdint.h>

// Syscalls do XNU (High Sierra) — números essenciais
#define SYS_exit         1
#define SYS_read         3
#define SYS_write        4
#define SYS_open         5
#define SYS_close        6
#define SYS_access       33
#define SYS_getpid       20
#define SYS_getuid       24
#define SYS_geteuid      25
#define SYS_getgid       47
#define SYS_getegid      48
#define SYS_ioctl        54
#define SYS_gettimeofday 116
#define SYS_mmap         197
#define SYS_munmap       73
#define SYS_mprotect     74
#define SYS_stat         188
#define SYS_fstat        189
#define SYS_lstat        199
#define SYS_sigaction    134
#define SYS_sigreturn    173

extern void vga_putchar(char c);
extern void vga_puts(const char *s);
extern char keyboard_read(void);

static inline int str_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

struct timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    vga[0] = (0x0E << 8) | ('0' + (num / 100 % 10));
    vga[1] = (0x0E << 8) | ('0' + (num / 10 % 10));
    vga[2] = (0x0E << 8) | ('0' + (num % 10));
    switch (num) {
        case SYS_write: {
            int fd = (int)a1;
            const char *buf = (const char *)a2;
            int count = (int)a3;
            if (fd == 1 || fd == 2) {
                for (int i = 0; i < count; i++) vga_putchar(buf[i]);
                return count;
            }
            return -1;
        }
        case SYS_read: {
            int fd = (int)a1;
            char *buf = (char *)a2;
            int count = (int)a3;
            if (fd == 0 && buf && count > 0) {
                int i;
                for (i = 0; i < count; i++) {
                    buf[i] = keyboard_read();
                    if (buf[i] == '\n') { i++; break; }
                }
                return i;
            }
            return 0;
        }
        case SYS_open: {
            const char *path = (const char *)a1;
            if (str_equal(path, "/dev/tty")) return 0;
            return -1;
        }
        case SYS_close:
            return 0;
        case SYS_access: {
            const char *path = (const char *)a1;
            if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin") || str_equal(path, "/dev/stdout") || str_equal(path, "/dev/stderr")) return 0;
            return -1;
        }
        case SYS_fstat:
        case SYS_lstat:
        case SYS_stat: {
            if ((int)a1 >= 0 && (int)a1 <= 2) return 0;
            return -1;
        }
        case SYS_mmap:
            return (uint64_t)mmap_user((void *)a1, a2, (int)a3, (int)a4);
        case SYS_munmap:
            return munmap_user((void *)a1, a2);
        case SYS_mprotect:
            return 0;
        case SYS_getpid:
            return 1;
        case SYS_getuid:
        case SYS_geteuid:
        case SYS_getgid:
        case SYS_getegid:
            return 0;
        case SYS_ioctl:
            return 0;
        case SYS_sigaction:
        case SYS_sigreturn:
            return 0;
        case SYS_gettimeofday: {
            struct timeval *tv = (struct timeval *)a1;
            if (tv) {
                tv->tv_sec = 0;
                tv->tv_usec = 0;
            }
            return 0;
        }
        default:
            vga_puts("Unknown syscall\n");
            return -1;
    }
}
