void _start(void) {
    /* write(1, msg, len) */
    const char msg[] = "Hello from musl ELF!\n";
    long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "a"(1), "D"(1), "S"((long)msg), "d"((long)sizeof(msg)-1)
        : "rcx", "r11", "memory");
    /* exit(0) */
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "a"(60), "D"(0)
        : "rcx", "r11", "memory");
    for (;;);
}
