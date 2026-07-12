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

#define HEAP_SIZE 65536
static char _heap[HEAP_SIZE];
static int _hinit;

#define HDR_SZ ((int)sizeof(unsigned long))
#define USED   1UL
#define AMASK  (~USED)
#define NEXT(b)  (*(void**)((char*)(b) + HDR_SZ))
#define ALIGN8(n) (((n) + 7) & ~7)
#define MMAP_THRESH 2048

static void *freep;

static void hinit(void) {
    if (_hinit) return;
    _hinit = 1;
    unsigned long *h = (unsigned long *)_heap;
    *h = HEAP_SIZE - HDR_SZ;
    NEXT(h) = h;
    freep = h;
}

void *malloc(int n) {
    if (n <= 0) return 0;
    if (!_hinit) hinit();

    if (n >= MMAP_THRESH) {
        int total = n + HDR_SZ;
        total = (total + 4095) & ~4095;
        unsigned long *h = (unsigned long *)_syscall(197, 0, total, 3, 0);
        if (!h) return 0;
        *h = (unsigned long)total | USED;
        return (char*)h + HDR_SZ;
    }

    n = ALIGN8(n);
    int need = n + HDR_SZ;

    unsigned long *prev = (unsigned long *)freep;
    unsigned long *curr = (unsigned long *)NEXT(prev);

    for (;;) {
        int csize = (int)(*curr & AMASK);
        if (csize >= need) {
            if (csize >= need + HDR_SZ + 16) {
                unsigned long *new = (unsigned long *)((char*)curr + need);
                *new = (unsigned long)(csize - need);
                NEXT(new) = NEXT(curr);
                *curr = (unsigned long)need | USED;
                NEXT(prev) = new;
            } else {
                *curr |= USED;
                NEXT(prev) = NEXT(curr);
            }
            freep = prev;
            return (char*)curr + HDR_SZ;
        }
        prev = curr;
        curr = (unsigned long *)NEXT(curr);
        if (curr == freep) break;
    }
    return 0;
}

void free(void *p) {
    if (!p) return;
    unsigned long *h = (unsigned long *)((char*)p - HDR_SZ);
    if ((char*)h < _heap || (char*)h >= _heap + HEAP_SIZE) {
        int total = (int)(*h & AMASK);
        _syscall(73, (long)h, total, 0, 0);
        return;
    }
    *h &= ~USED;

    unsigned long *prev = (unsigned long *)freep;
    unsigned long *curr = (unsigned long *)NEXT(prev);

    for (;;) {
        if (h > prev && h < curr) break;
        if (prev > curr && (h > prev || h < curr)) break;
        prev = curr;
        curr = (unsigned long *)NEXT(curr);
        if (prev == freep) break;
    }

    NEXT(h) = curr;
    NEXT(prev) = h;

    if ((char*)h + (int)(*h & AMASK) == (char*)curr) {
        *h = (*h & AMASK) + (*curr & AMASK);
        NEXT(h) = NEXT(curr);
    }
    if ((char*)prev + (int)(*prev & AMASK) == (char*)h) {
        *prev = (*prev & AMASK) + (*h & AMASK);
        NEXT(prev) = NEXT(h);
    }

    freep = prev;
}

void *calloc(int n, int size) {
    int total = n * size;
    void *p = malloc(total);
    if (p) {
        char *cp = (char *)p;
        for (int i = 0; i < total; i++) cp[i] = 0;
    }
    return p;
}

void *realloc(void *p, int n) {
    if (!p) return malloc(n);
    if (n <= 0) { free(p); return 0; }
    unsigned long *h = (unsigned long *)((char*)p - HDR_SZ);
    int old = (int)(*h & AMASK) - HDR_SZ;
    if (old >= n) return p;
    void *new = malloc(n);
    if (!new) return 0;
    char *sp = (char*)p;
    char *dp = (char*)new;
    int copy = old < n ? old : n;
    for (int i = 0; i < copy; i++) dp[i] = sp[i];
    free(p);
    return new;
}

void *mmap(void *addr, int length, int prot, int flags) {
    (void)addr; (void)prot; (void)flags;
    int total = (length + 4095) & ~4095;
    return (void*)_syscall(197, 0, total, 3, 0);
}

int munmap(void *addr, int length) {
    int total = (length + 4095) & ~4095;
    return (int)_syscall(73, (long)addr, total, 0, 0);
}

void exit(int code) {
    _syscall(1, code, 0, 0, 0);
    for (;;) __asm__("hlt");
}
