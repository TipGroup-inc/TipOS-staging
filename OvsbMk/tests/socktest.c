#include <stdint.h>
#include <stddef.h>

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
#define SYS_exit_group 231
#define SYS_socket  41
#define SYS_bind    49
#define SYS_listen  50
#define SYS_connect 42
#define SYS_accept4 288
#define SYS_socketpair 53
#define SYS_sendto  44
#define SYS_recvfrom 45
#define AF_UNIX 1
#define SOCK_STREAM 1
#define O_NONBLOCK 0x800

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void puts(const char *s) { syscall(SYS_write, 1, (long)s, slen(s), 0, 0); }
static void nl(void) { syscall(SYS_write, 1, (long)"\n", 1, 0, 0); }

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

void _start(void) {
    puts("[socktest] init");
    nl();

    /* ~~ socketpair test ~~ */
    int fds[2];
    long r = syscall(SYS_socketpair, AF_UNIX, SOCK_STREAM, 0, (long)fds, 0);
    puts("socketpair: "); 
    if (r == 0) {
        puts("OK fds=");
        char b[4]; b[0] = '0' + fds[0]; b[1] = ','; b[2] = '0' + fds[1]; b[3] = 0;
        puts(b); nl();
        /* ~~ write on fds[1], read on fds[0] ~~ */
        const char *msg = "ping<3";
        long w = syscall(SYS_write, fds[1], (long)msg, 6, 0, 0);
        puts("write: "); 
        char wb[4]; wb[0] = '0' + w; wb[1] = 0; puts(wb); nl();
        char buf[16] = {0};
        long rd = syscall(0, fds[0], (long)buf, 16, 0, 0); /* read */
        puts("read: "); puts(buf); nl();
        if (buf[0] == 'p' && buf[1] == 'i') puts("[socktest] SOCKETPAIR OK <3\n");
        else puts("[socktest] SOCKETPAIR FAIL\n");
    } else {
        puts("FAIL ret="); nl();
    }

    /* ~~ bind/listen/connect/accept test ~~ */
    int ls = syscall(SYS_socket, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    puts("socket(listen): "); 
    char lb[4]; lb[0] = '0' + ls; lb[1] = 0; puts(lb); nl();
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    int i = 0;
    const char *p = "/tmp/test.sock";
    while (p[i] && i < 107) { sa.sun_path[i] = p[i]; i++; }
    sa.sun_path[i] = 0;
    r = syscall(SYS_bind, ls, (long)&sa, (long)(2 + i), 0, 0);
    puts("bind: "); 
    char bb[4]; bb[0] = '0' + r; bb[1] = 0; puts(bb); nl();
    r = syscall(SYS_listen, ls, 8, 0, 0, 0);
    puts("listen: "); 
    char l2[4]; l2[0] = '0' + r; l2[1] = 0; puts(l2); nl();

    /* ~~ client ~~ */
    int cl = syscall(SYS_socket, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    r = syscall(SYS_connect, cl, (long)&sa, (long)(2 + i), 0, 0);
    puts("connect: "); 
    char cb[4]; cb[0] = '0' + r; cb[1] = 0; puts(cb); nl();
    int acc = syscall(SYS_accept4, ls, 0, 0, 0, 0);
    puts("accept4: "); 
    char ab[4]; ab[0] = '0' + acc; ab[1] = 0; puts(ab); nl();
    if (acc > 0) {
        const char *m2 = "server->client<3";
        syscall(SYS_sendto, acc, (long)m2, 14, 0, 0);
        char buf2[32] = {0};
        long rd2 = syscall(0, cl, (long)buf2, 32, 0, 0);
        puts("client got: "); puts(buf2); nl();
        if (buf2[0] == 's') puts("[socktest] CONNECT/ACCEPT OK <3\n");
        else puts("[socktest] CONNECT/ACCEPT FAIL\n");
    } else {
        puts("[socktest] accept4 FAIL\n");
    }

    syscall(SYS_exit_group, 0, 0, 0, 0, 0);
    for (;;);
}
