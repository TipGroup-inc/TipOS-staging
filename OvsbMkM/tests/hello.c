void _start(void) {
    // Escrever diretamente na VGA (sem syscall)
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    vga[160] = (0x0E << 8) | 'H';  // H amarelo
    vga[161] = (0x0E << 8) | 'i';  // i amarelo
    
    while(1);
}
