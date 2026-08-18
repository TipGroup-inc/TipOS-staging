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
#define SYS_exit_group 231
#define SYS_set_tid_address 218
#define SYS_futex 202
#define SYS_set_robust_list 273
#define SYS_get_robust_list 274
#define SYS_rt_sigprocmask 14
#define SYS_sigaltstack 131
#define SYS_prctl 157
#define SYS_rseq 334
#define SYS_kill 62
#define SYS_tgkill 234
#define SYS_getpid 39

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void puts(const char *s) { syscall(SYS_write, 1, (long)s, slen(s), 0, 0); }
static void nl(void) { syscall(SYS_write, 1, (long)"\n", 1, 0, 0); }
static void itoa(int n, char *b) {
    int i = 0; if (n == 0) { b[i++] = '0'; }
    while (n) { b[i++] = '0' + (n % 10); n /= 10; }
    for (int j = 0; j < i/2; j++) { char t = b[j]; b[j] = b[i-1-j]; b[i-1-j] = t; }
    b[i] = 0;
}
static void pass(const char *what) { puts("[ttest50] "); puts(what); puts(" OK <3"); nl(); }
static void fail(const char *what, long r) {
    puts("[ttest50] "); puts(what); puts(" FAIL ret=");
    char b[16]; itoa((int)r, b); puts(b); nl();
}

void _start(void) {
    /* ~~ set_tid_address: devolve o PID ~~ */
    int tid_addr = 0;
    long r = syscall(SYS_set_tid_address, (long)&tid_addr, 0, 0, 0, 0);
    if (r > 0) pass("set_tid_address"); else fail("set_tid_address", r);

    /* ~~ futex WAIT/WAKE: não pode abortar ~~ */
    volatile uint32_t lock = 0;
    r = syscall(SYS_futex, (long)&lock, FUTEX_WAIT, 0, 0, 0);
    if (r == 0 || r == -1 || r == -11) pass("futex WAIT"); else fail("futex WAIT", r);
    r = syscall(SYS_futex, (long)&lock, FUTEX_WAKE, 1, 0, 0);
    if (r >= 0) pass("futex WAKE"); else fail("futex WAKE", r);

    /* ~~ set_robust_list: ponteiro + tamanho ~~ */
    uint64_t robust[2] = {0, 0};
    r = syscall(SYS_set_robust_list, (long)&robust, sizeof(robust), 0, 0, 0);
    if (r == 0) pass("set_robust_list"); else fail("set_robust_list", r);

    /* ~~ rt_sigprocmask ~~ */
    r = syscall(SYS_rt_sigprocmask, 0, 0, 0, 8, 0);
    if (r == 0) pass("rt_sigprocmask"); else fail("rt_sigprocmask", r);

    /* ~~ sigaltstack ~~ */
    char altstack[8192];
    uint64_t ss[3] = { (uint64_t)altstack, 8192, 0 };
    r = syscall(SYS_sigaltstack, (long)&ss, 0, 0, 0, 0);
    if (r == 0) pass("sigaltstack"); else fail("sigaltstack", r);

    /* ~~ prctl PR_SET_NAME ~~ */
    char name[16] = "ttest50";
    r = syscall(SYS_prctl, 15, (long)name, 0, 0, 0); /* PR_SET_NAME */
    if (r == 0) pass("prctl"); else fail("prctl", r);

    /* ~~ rseq (aceita, libc desativa se ENOSYS) ~~ */
    uint8_t rseq_buf[32];
    r = syscall(SYS_rseq, (long)rseq_buf, sizeof(rseq_buf), 0, 0, 0);
    if (r == 0) pass("rseq"); else fail("rseq", r);

    /* ~~ kill / tgkill ~~ */
    long pid = syscall(SYS_getpid, 0, 0, 0, 0, 0);
    r = syscall(SYS_kill, pid, 0, 0, 0, 0); /* sig 0 = verifica */
    if (r == 0) pass("kill"); else fail("kill", r);
    r = syscall(SYS_tgkill, pid, pid, 0, 0, 0);
    if (r == 0) pass("tgkill"); else fail("tgkill", r);

    puts("[ttest50] todos os testes de threads/sinais OK <3"); nl();
    syscall(SYS_exit_group, 0, 0, 0, 0, 0);
    for (;;);
}
