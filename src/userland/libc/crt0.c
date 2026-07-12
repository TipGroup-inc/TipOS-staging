extern int main(int argc, char **argv);
extern char _bss_start, _bss_end;

__attribute__((section(".text.start")))
void _start(void) {
    for (char *p = &_bss_start; p < &_bss_end; p++) *p = 0;
    main(0, 0);
}
