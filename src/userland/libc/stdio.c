#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

FILE __stdin_file  = { .fd = 0 };
FILE __stdout_file = { .fd = 1 };
FILE __stderr_file = { .fd = 2 };

static long _syscall(long num, long a1, long a2, long a3, long a4) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4)
        : "memory"
    );
    return ret;
}

int open(const char *path, int flags) {
    return _syscall(5, (long)path, flags, 0, 0);
}

int close(int fd) {
    return _syscall(6, fd, 0, 0, 0);
}

int read(int fd, void *buf, int count) {
    return _syscall(3, fd, (long)buf, count, 0);
}

int write(int fd, const void *buf, int count) {
    return _syscall(4, fd, (long)buf, count, 0);
}

int lseek(int fd, int offset, int whence) {
    return _syscall(202, fd, offset, whence, 0);
}

int unlink(const char *path) {
    return _syscall(10, (long)path, 0, 0, 0);
}

int mkdir(const char *path) {
    return _syscall(136, (long)path, 0, 0, 0);
}

int rmdir(const char *path) {
    return _syscall(137, (long)path, 0, 0, 0);
}

int stat(const char *path, struct stat *buf) {
    return _syscall(188, (long)path, (long)buf, 0, 0);
}

int fstat(int fd, struct stat *buf) {
    return _syscall(189, fd, (long)buf, 0, 0);
}

int kbhit(void) {
    return _syscall(198, 0, 0, 0, 0);
}

void putchar(char c) {
    write(1, &c, 1);
}

void puts(const char *s) {
    write(1, s, strlen(s));
    write(1, "\n", 1);
}

char getchar(void) {
    char c;
    read(0, &c, 1);
    return c;
}

char *gets(char *buf) {
    int i = 0;
    while (1) {
        char c = getchar();
        if (c == '\n') break;
        if (c == '\b') { if (i > 0) i--; continue; }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return buf;
}

FILE *fopen(const char *path, const char *mode) {
    (void)mode;
    int fd = open(path, 0);
    if (fd < 0) return 0;
    static FILE f;
    f.fd = fd;
    return &f;
}

int fclose(FILE *f) {
    return close(f->fd);
}

int fread(void *buf, int size, int count, FILE *f) {
    int r = read(f->fd, buf, size * count);
    if (r <= 0) return 0;
    return r / size;
}

int fwrite(const void *buf, int size, int count, FILE *f) {
    int r = write(f->fd, buf, size * count);
    if (r <= 0) return 0;
    return r / size;
}

char *fgets(char *buf, int n, FILE *f) {
    int i = 0;
    while (i < n - 1) {
        char c;
        if (read(f->fd, &c, 1) <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i > 0 ? buf : 0;
}

int fputs(const char *s, FILE *f) {
    return write(f->fd, s, strlen(s)) > 0 ? 0 : -1;
}

int fputc(int c, FILE *f) {
    char ch = c;
    return write(f->fd, &ch, 1) == 1 ? c : -1;
}

int sscanf(const char *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int matched = 0;
    while (*fmt) {
        if (*fmt == ' ') { fmt++; continue; }
        if (*fmt != '%') {
            if (*s == *fmt) { s++; fmt++; continue; }
            break;
        }
        fmt++;
        switch (*fmt) {
            case 'd': {
                int *v = va_arg(ap, int *);
                while (*s == ' ') s++;
                int sign = 1;
                if (*s == '-') { sign = -1; s++; }
                else if (*s == '+') s++;
                int n = 0;
                while (isdigit(*s)) { n = n * 10 + (*s - '0'); s++; }
                *v = sign * n;
                matched++;
                fmt++;
                break;
            }
            case 'c': {
                char *v = va_arg(ap, char *);
                *v = *s++;
                matched++;
                fmt++;
                break;
            }
            default:
                fmt++;
                break;
        }
    }
    va_end(ap);
    return matched;
}

static void _print_dec(unsigned long n) {
    char buf[20], *p = buf;
    do { *p++ = '0' + (n % 10); n /= 10; } while (n);
    while (p > buf) putchar(*--p);
}

static void _print_hex(unsigned long n) {
    const char *hex = "0123456789abcdef";
    char buf[20], *p = buf;
    do { *p++ = hex[n & 0xF]; n >>= 4; } while (n);
    while (p > buf) putchar(*--p);
}

int vsnprintf(char *buf, int n, const char *fmt, va_list ap) {
    if (!buf || n <= 0) return 0;
    int p = 0;
    for (const char *f = fmt; *f && p < n - 1; f++) {
        if (*f != '%') { buf[p++] = *f; continue; }
        f++;
        int lflag = 0;
        while (*f == 'l') { lflag = 1; f++; }
        switch (*f) {
            case 'd': {
                long v = lflag ? va_arg(ap, long) : (long)va_arg(ap, int);
                if (v < 0) { buf[p++] = '-'; v = -v; }
                char tmp[24], *t = tmp;
                unsigned long uv = (unsigned long)v;
                do { *t++ = '0' + (uv % 10); uv /= 10; } while (uv);
                while (t > tmp && p < n - 1) buf[p++] = *--t;
                break;
            }
            case 'u': {
                unsigned long v = lflag ? va_arg(ap, unsigned long)
                                        : (unsigned long)va_arg(ap, unsigned);
                char tmp[24], *t = tmp;
                do { *t++ = '0' + (v % 10); v /= 10; } while (v);
                while (t > tmp && p < n - 1) buf[p++] = *--t;
                break;
            }
            case 'x':
            case 'X': {
                unsigned long v = lflag ? va_arg(ap, unsigned long)
                                        : (unsigned long)va_arg(ap, unsigned);
                const char *hex = "0123456789abcdef";
                char tmp[24], *t = tmp;
                do { *t++ = hex[v & 0xF]; v >>= 4; } while (v);
                while (t > tmp && p < n - 1) buf[p++] = *--t;
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && p < n - 1) buf[p++] = *s++;
                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                buf[p++] = c;
                break;
            }
            case '%':
                buf[p++] = '%';
                break;
            default:
                buf[p++] = '%';
                if (*f) buf[p++] = *f;
                break;
        }
    }
    buf[p] = '\0';
    return p;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, 4096, fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { fputc(*p, f); continue; }
        p++;
        switch (*p) {
            case 'd': {
                int v = va_arg(ap, int);
                if (v < 0) { fputc('-', f); v = -v; }
                char buf[12], *b = buf;
                unsigned u = v;
                do { *b++ = '0' + (u % 10); u /= 10; } while (b > buf);
                while (b > buf) fputc(*--b, f);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                while (*s) fputc(*s++, f);
                break;
            }
            case 'c':
                fputc(va_arg(ap, int), f);
                break;
            case '%':
                fputc('%', f);
                break;
        }
    }
    va_end(ap);
    return 0;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { putchar(*p); continue; }
        p++;
        switch (*p) {
            case 'd': {
                int v = va_arg(ap, int);
                if (v < 0) { putchar('-'); v = -v; }
                _print_dec((unsigned)v);
                break;
            }
            case 'u':
                _print_dec(va_arg(ap, unsigned));
                break;
            case 'x':
            case 'X':
                _print_hex(va_arg(ap, unsigned));
                break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                while (*s) putchar(*s++);
                break;
            }
            case 'c':
                putchar(va_arg(ap, int));
                break;
            case '%':
                putchar('%');
                break;
        }
    }
    va_end(ap);
    return 0;
}
