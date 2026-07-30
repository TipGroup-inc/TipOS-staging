/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stdio.c ~ funcoes anotadas: 30
 */

// ~~ stdio.c ~~ Libc: entrada e saída padrão~
// Implementa tudo via int 0x80 (syscall). Sem bufferização complexa,
// sem FILE* mágico - só o fd e fé~ ♡
//
// Syscalls usados:
//   read(3), write(4), open(5), close(6), lseek(199)
//   stat(188), fstat(189), kbhit(198), mkdir(136), rmdir(137)

/* ~*~ stdio.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

FILE __stdin_file  = { .fd = 0 };
FILE __stdout_file = { .fd = 1 };
FILE __stderr_file = { .fd = 2 };

// ~~ _syscall ~~
// Chamada de syscall via int 0x80 (Linux-style).
// num = número do syscall (traduzido pelo kernel~)
// a1-a4 = argumentos em rdi, rsi, rdx, rcx
// retorno em rax. Usa constraint "=a" pra pegar o resultado~ ☆
/* ~ essa demorou pra debugar, respeita ~ */
static long _syscall(long num, long a1, long a2, long a3, long a4) {
    long ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "c"(a4)
        : "r11", "memory"
    );
    return ret;
}

// ~~ open ~~
// Syscall open(2): abre arquivo no path com flags.
// Traduzido pelo kernel: nosso syscall 5 = TipOS open~
/* ~~ open ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int open(const char *path, int flags) {
    return _syscall(5, (long)path, flags, 0, 0);
}

// ~~ close ~~
// System call close(2): libera o file descriptor.
// Importante chamar pra não vazar fd~ (recursos são finitos!)
/* ~~ close ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int close(int fd) {
    return _syscall(6, fd, 0, 0, 0);
}

/* ~~ Lendo coisinhas~ toma cuidado pra não ler lixo! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int read(int fd, void *buf, int count) {
    return _syscall(3, fd, (long)buf, count, 0);
}

/* ~~ Escrevendo~~ não vai corromper nada, vai? >_< */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int write(int fd, const void *buf, int count) {
    return _syscall(4, fd, (long)buf, count, 0);
}

/* ~~ lseek ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int lseek(int fd, int offset, int whence) {
    return _syscall(199, fd, offset, whence, 0);
}

/* ~~ unlink ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int unlink(const char *path) {
    return _syscall(10, (long)path, 0, 0, 0);
}

/* ~~ mkdir ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int mkdir(const char *path) {
    return _syscall(136, (long)path, 0, 0, 0);
}

/* ~~ rmdir ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int rmdir(const char *path) {
    return _syscall(137, (long)path, 0, 0, 0);
}

/* ~~ stat ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int stat(const char *path, struct stat *buf) {
    return _syscall(188, (long)path, (long)buf, 0, 0);
}

/* ~~ fstat ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int fstat(int fd, struct stat *buf) {
    return _syscall(189, fd, (long)buf, 0, 0);
}

/* ~~ kbhit ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int kbhit(void) {
    return _syscall(198, 0, 0, 0, 0);
}

/* ~~ putchar ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void putchar(char c) {
    write(1, &c, 1);
}

/* ~~ puts ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void puts(const char *s) {
    write(1, s, strlen(s));
    write(1, "\n", 1);
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char getchar(void) {
    char c;
    read(0, &c, 1);
    return c;
}

/* ~ cuidado que essa aqui morde ~ */
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

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
FILE *fopen(const char *path, const char *mode) {
    (void)mode;
    int fd = open(path, 0);
    if (fd < 0) return 0;
    static FILE f;
    f.fd = fd;
    return &f;
}

/* ~~ fclose ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int fclose(FILE *f) {
    return close(f->fd);
}

/* ~~ Lendo coisinhas~ toma cuidado pra não ler lixo! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int fread(void *buf, int size, int count, FILE *f) {
    int r = read(f->fd, buf, size * count);
    if (r <= 0) return 0;
    return r / size;
}

/* ~~ Escrevendo~~ não vai corromper nada, vai? >_< */
/* ~ essa demorou pra debugar, respeita ~ */
int fwrite(const void *buf, int size, int count, FILE *f) {
    int r = write(f->fd, buf, size * count);
    if (r <= 0) return 0;
    return r / size;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
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

/* ~~ fputs ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int fputs(const char *s, FILE *f) {
    return write(f->fd, s, strlen(s)) > 0 ? 0 : -1;
}

/* ~~ fputc ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int fputc(int c, FILE *f) {
    char ch = c;
    return write(f->fd, &ch, 1) == 1 ? c : -1;
}

/* ~~ sscanf ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
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

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void _print_dec(unsigned long n) {
    char buf[20], *p = buf;
    do { *p++ = '0' + (n % 10); n /= 10; } while (n);
    while (p > buf) putchar(*--p);
}

/* ~ essa demorou pra debugar, respeita ~ */
static void _print_hex(unsigned long n) {
    const char *hex = "0123456789abcdef";
    char buf[20], *p = buf;
    do { *p++ = hex[n & 0xF]; n >>= 4; } while (n);
    while (p > buf) putchar(*--p);
}

/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ essa demorou pra debugar, respeita ~ */
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

/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, 4096, fmt, ap);
    va_end(ap);
    return n;
}

/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ cuidado que essa aqui morde ~ */
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

/* ~~ Mostrando pro mundo~~ lindo demais! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
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




/* ♥ stdio.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
