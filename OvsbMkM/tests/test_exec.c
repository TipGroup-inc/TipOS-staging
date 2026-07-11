void kmain(void);

// Código assembly que escreve 'H' na VGA
unsigned char test_code[] = {
    0x48, 0xC7, 0x04, 0x25, 0x00, 0x80, 0x0B, 0x00,  // mov rax, 0xB8000
    0x48, 0xC7, 0x00, 0x48, 0x0A, 0x00, 0x00,        // mov word [rax], 0x0A48 ('H' verde)
    0xF4,                                               // hlt
    0xEB, 0xFE                                          // jmp $ (loop infinito)
};
