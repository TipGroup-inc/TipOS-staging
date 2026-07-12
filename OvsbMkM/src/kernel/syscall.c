#include "idt.h"
#include "memory.h"
#include "fat32.h"
#include <stdint.h>

// Syscall numbers
#define SYS_exit         1
#define SYS_read         3
#define SYS_write        4
#define SYS_open         5
#define SYS_close        6
#define SYS_unlink       10
#define SYS_access       33
#define SYS_getpid       20
#define SYS_getuid       24
#define SYS_geteuid      25
#define SYS_getgid       47
#define SYS_getegid      48
#define SYS_ioctl        54
#define SYS_gettimeofday 116
#define SYS_sigaction    134
#define SYS_sigreturn    173
#define SYS_munmap       73
#define SYS_mprotect     74
#define SYS_stat         188
#define SYS_fstat        189
#define SYS_lstat        199
#define SYS_mmap         197
#define SYS_kbhit        198
#define SYS_lseek        200
#define SYS_mkdir2       136
#define SYS_rmdir2       137

// File descriptor table
#define MAX_FDS 16
static struct { int used; char name[256]; uint32_t pos; } fds[MAX_FDS];

// Open flags
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0x200

extern void vga_putchar(char c);
extern void vga_puts(const char *s);
extern char keyboard_read(void);
extern int keyboard_avail(void);

static inline int str_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

struct timeval { uint64_t tv_sec; uint64_t tv_usec; };

struct stat { uint32_t st_size; };

static int alloc_fd(void) {
    for (int i = 3; i < MAX_FDS; i++) if (!fds[i].used) return i;
    return -1;
}

static void close_fd(int fd) {
    if (fd >= 3 && fd < MAX_FDS) fds[fd].used = 0;
}

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a4;
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    vga[0] = (0x0E << 8) | ('0' + (num / 100 % 10));
    vga[1] = (0x0E << 8) | ('0' + (num / 10 % 10));
    vga[2] = (0x0E << 8) | ('0' + (num % 10));
    switch (num) {
        case SYS_exit:
            return 0;
        case SYS_write: {
            int fd = (int)a1;
            const char *buf = (const char *)a2;
            int count = (int)a3;
            if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
                int r = fat32_write_file(fds[fd].name, (const uint8_t *)buf, count);
                if (r >= 0) return count;
                return -1;
            }
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
            if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
                static uint8_t tmp[4096];
                int r = fat32_read_file(fds[fd].name, tmp, 4096);
                if (r < 0) return -1;
                int off = fds[fd].pos;
                int n = (count < r - off) ? count : (r - off);
                if (n < 0) return 0;
                for (int i = 0; i < n; i++) buf[i] = tmp[off + i];
                fds[fd].pos += n;
                return n;
            }
            return -1;
        }
        case SYS_open: {
            const char *path = (const char *)a1;
            if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin")) return 0;
            if (str_equal(path, "/dev/stdout")) return 1;
            if (str_equal(path, "/dev/stderr")) return 2;
            int fd = alloc_fd();
            if (fd < 0) return -1;
            int i = 0;
            while (path[i] && i < 255) { fds[fd].name[i] = path[i]; i++; }
            fds[fd].name[i] = '\0';
            fds[fd].pos = 0;
            fds[fd].used = 1;
            return fd;
        }
        case SYS_close:
            close_fd((int)a1);
            return 0;
        case SYS_access: {
            const char *path = (const char *)a1;
            if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin") ||
                str_equal(path, "/dev/stdout") || str_equal(path, "/dev/stderr")) return 0;
            return 0;
        }
        case SYS_fstat: {
            struct stat *st = (struct stat *)a2;
            if (!st) return -1;
            int fd = (int)a1;
            if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
                uint32_t size; uint8_t attr;
                if (fat32_stat(fds[fd].name, &size, &attr, NULL, NULL) == 0) { st->st_size = size; return 0; }
            }
            if (fd >= 0 && fd <= 2) { st->st_size = 0; return 0; }
            return -1;
        }
        case SYS_lstat:
        case SYS_stat: {
            const char *path = (const char *)a1;
            struct stat *st = (struct stat *)a2;
            if (!path || !st) return -1;
            uint32_t size; uint8_t attr;
            if (fat32_stat(path, &size, &attr, NULL, NULL) == 0) {
                st->st_size = size;
                return 0;
            }
            return -1;
        }
        case SYS_lseek: {
            int fd = (int)a1;
            int off = (int)a2;
            int whence = (int)a3;
            if (fd < 3 || fd >= MAX_FDS || !fds[fd].used) return -1;
            switch (whence) {
                case 0: fds[fd].pos = off; break;
                case 1: fds[fd].pos += off; break;
                case 2: {
                    uint32_t size = 0;
                    uint8_t attr;
                    if (fat32_stat(fds[fd].name, &size, &attr, NULL, NULL) == 0) fds[fd].pos = size + off;
                    break;
                }
                default: return -1;
            }
            return fds[fd].pos;
        }
        case SYS_unlink:
            if (fat32_delete_file((const char *)a1) == 0) return 0;
            return -1;
        case SYS_mkdir2:
            if (fat32_mkdir((const char *)a1) == 0) return 0;
            return -1;
        case SYS_rmdir2:
            if (fat32_rmdir((const char *)a1) == 0) return 0;
            return -1;
        case SYS_mmap:
            return (uint64_t)mmap_user((void *)a1, a2, (int)a3, (int)a4);
        case SYS_munmap:
            return munmap_user((void *)a1, a2);
        case SYS_mprotect:
            return 0;
        case SYS_kbhit:
            return keyboard_avail();
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
            if (tv) { tv->tv_sec = 0; tv->tv_usec = 0; }
            return 0;
        }
        default:
            vga_puts("Unknown syscall\n");
            return -1;
    }
}
