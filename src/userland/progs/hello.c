__attribute__((noreturn)) void _start(void) {
    __asm__ volatile (
        "mov $4, %%rax\n\t"
        "mov $1, %%rdi\n\t"
        "lea 1f(%%rip), %%rsi\n\t"
        "mov $22, %%rdx\n\t"
        "int $0x80\n\t"
        "ret\n\t"
        "1: .ascii \"Hello from userland!\\n\""
        : : : "rax","rdi","rsi","rdx"
    );
    __builtin_unreachable();
}
