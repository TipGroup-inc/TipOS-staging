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

int atoi(const char *s) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return sign * n;
}

char *itoa(int n, char *buf) {
    char *p = buf;
    unsigned u;
    if (n < 0) { *p++ = '-'; u = -n; } else u = n;
    char tmp[12], *t = tmp;
    do { *t++ = '0' + (u % 10); u /= 10; } while (u);
    while (t > tmp) *p++ = *--t;
    *p = '\0';
    return buf;
}

static char _heap[65536];
static int _heap_pos = 0;

void *malloc(int n) {
    if (_heap_pos + n > 65536) return 0;
    void *p = _heap + _heap_pos;
    _heap_pos += n;
    return p;
}

void free(void *p) {
    (void)p;
}

void exit(int code) {
    __asm__ volatile (
        "mov $1, %%rax; mov %0, %%rdi; int $0x80"
        :: "r"((long)code) : "rax", "rdi"
    );
    __builtin_unreachable();
}
