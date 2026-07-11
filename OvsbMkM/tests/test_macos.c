// Simula um binário do macOS chamando write()
void start(void) {
    const char *msg = "MacOS via OvsbMkM!";
    
    // Chamar syscall write(1, msg, 13)
    // Usando instrução syscall (modo 64-bit)
    __asm__ volatile (
        "mov $4, %%rax\n"    // SYS_write = 4
        "mov $1, %%rdi\n"    // fd = 1 (stdout)
        "mov %0, %%rsi\n"    // buf = msg
        "mov $13, %%rdx\n"   // count = 13
        "int $0x80\n"        // Chamar kernel
        :
        : "r"(msg)
        : "rax", "rdi", "rsi", "rdx"
    );
    
    // Chamar exit(0)
    __asm__ volatile (
        "mov $1, %%rax\n"    // SYS_exit = 1
        "xor %%rdi, %%rdi\n" // status = 0
        "int $0x80\n"
    );
}
