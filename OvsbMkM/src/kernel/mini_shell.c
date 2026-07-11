// minimal mini_shell: uses int 0x80 syscalls for read/write
void _start() {
    char buf[256];
    int n;
    while (1) {
        __asm__ volatile (
            "mov $1, %%rax\n\t" // SYS_write (placeholder; we'll use int 0x80)
            "mov $1, %%rdi\n\t"
            "lea prompt(%%rip), %%rsi\n\t"
            "mov $2, %%rdx\n\t"
            "int $0x80\n\t"
            : : : "rax","rdi","rsi","rdx"
        );
        // read
        __asm__ volatile (
            "mov $0, %%rax\n\t" // SYS_read
            "mov $0, %%rdi\n\t"
            "lea buf(%%rip), %%rsi\n\t"
            "mov $255, %%rdx\n\t"
            "int $0x80\n\t"
            "mov %%eax, %0\n\t"
            : "=r" (n) :: "rax","rdi","rsi","rdx"
        );
        if (n > 0) {
            // write back
            __asm__ volatile (
                "mov $1, %%rax\n\t"
                "mov $1, %%rdi\n\t"
                "lea buf(%%rip), %%rsi\n\t"
                "mov %0, %%rdx\n\t"
                "int $0x80\n\t"
                : : "r" (n) : "rax","rdi","rsi","rdx"
            );
        }
    }
}

const char prompt[] = "$ ";
char buf[256] = {0};
