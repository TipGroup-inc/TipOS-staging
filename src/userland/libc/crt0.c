extern int main(int argc, char **argv);
extern char _bss_start, _bss_end;

static void _exit(int code) {
    __asm__ volatile (
        "mov $1, %%rax; mov %0, %%rdi; int $0x80"
        :: "r"((long)code) : "rax", "rdi"
    );
    __builtin_unreachable();
}

__attribute__((section(".text.start")))
void _start(void) {
    for (char *p = &_bss_start; p < &_bss_end; p++) *p = 0;
    int ret = main(0, 0);
    _exit(ret);
}
