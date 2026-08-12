#include <stdint.h>
#include <stddef.h>

/* ~~ Inline syscall wrapper pros Linux syscalls ~~ */
static long syscall(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 asm("r10") = a4;
    register long r8  asm("r8")  = a5;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory");
    return ret;
}

#define SYS_write 1
#define SYS_ioctl 16
#define SYS_poll  7
#define SYS_exit  60
#define SYS_exit_group 231
#define SYS_gettimeofday 96

/* ~~ Linux structs ~~ */
struct winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
};

struct pollfd {
    int fd;
    short events;
    short revents;
};
#define POLLIN 1

/* ~~ termios ~~ */
#define TCGETS 0x5401
#define TCSETSW 0x5403
#define TIOCGWINSZ 0x5413

/* ~~ strlen ~~ */
static int slen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ~~ itoa ~~ */
static void itoa(int n, char *buf) {
    int i = 0, neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    do { buf[i++] = '0' + (n % 10); n /= 10; } while (n);
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = t;
    }
    buf[i] = '\0';
}

/* ~~ Saida ~~ */
static void puts(const char *s) { syscall(SYS_write, 1, (long)s, slen(s), 0, 0); }

/* ~~ \n ~~ */
static void nl(void) { syscall(SYS_write, 1, (long)"\n", 1, 0, 0); }

/* ~~ stdin read ~~ */
static char getch(void) {
    char c = 0;
    syscall(0, 0, (long)&c, 1, 0, 0); /* SYS_read */
    return c;
}

/* ~~ main ~~ */
void _start(void) {
    int row = 0, col = 0, cols = 80, rows = 25;

    /* ~~ TIOCGWINSZ ~~ */
    struct winsize ws;
    if (syscall(SYS_ioctl, 0, TIOCGWINSZ, (long)&ws, 0, 0) == 0) {
        if (ws.ws_row > 0) rows = ws.ws_row;
        if (ws.ws_col > 0) cols = ws.ws_col;
    }

    puts("\x1b[2J"); /* ~~ clear screen ~~ */
    puts("\x1b[H");  /* ~~ cursor home ~~ */

    puts("\r\n-= TipOS Terminal =- moe moe kyun~ <3");
    nl();

    char buf[80];
    int bi = 0;
    char prompt[16] = "> ";

    /* ~~ main loop: print prompt, read line, echo ~~ */
    while (1) {
        puts(prompt);
        bi = 0;
        while (1) {
            char c = getch();
            if (c == '\n') { buf[bi] = 0; nl(); break; }
            if (c == '\x03') { /* ^C */
                puts("^C\x1b[K");
                nl();
                syscall(SYS_exit_group, 0, 0, 0, 0, 0);
                for (;;);
            }
            if (c == 127 && bi > 0) { bi--; syscall(SYS_write, 1, (long)"\b \b", 3, 0, 0); continue; }
            if (c >= 32 && bi < 79) { buf[bi++] = c; syscall(SYS_write, 1, (long)&c, 1, 0, 0); }
        }

        if (bi == 0) continue;

        /* ~~ poll test ~~ */
        if (buf[0] == 'p' && buf[1] == 'o' && bi == 4) {
            struct pollfd pfd;
            pfd.fd = 0; pfd.events = POLLIN;
            long r = syscall(SYS_poll, (long)&pfd, 1, 1000, 0, 0);
            char rbuf[32];
            puts("poll: ");
            itoa((int)r, rbuf); puts(rbuf);
            puts(" ready | revents=");
            itoa(pfd.revents, rbuf); puts(rbuf);
            nl();
            continue;
        }

        /* ~~ ioctl test ~~ */
        if (buf[0] == 'w' && bi == 5) {
            struct winsize w;
            syscall(SYS_ioctl, 0, TIOCGWINSZ, (long)&w, 0, 0);
            char rbuf[32];
            puts("winsize: ");
            itoa(w.ws_row, rbuf); puts(rbuf);
            puts("x");
            itoa(w.ws_col, rbuf); puts(rbuf);
            nl();
            continue;
        }

        /* ~~ exit ~~ */
        if (buf[0] == 'e' && bi == 4) {
            puts("bye~ kyun! <3\n");
            syscall(SYS_exit, 0, 0, 0, 0, 0);
            for (;;);
        }

        /* ~~ echo ~~ */
        buf[bi] = 0;
        puts("voce disse: ");
        puts(buf);
        nl();
    }
}
