// Função chamada pelos handlers de interrupção
void idt_handler(int num, int err) {
    volatile unsigned short *vga = (unsigned short *)0xB8000;
    
    // Se for IRQ1 (teclado), mostra 'K' amarelo
    if (num == 33) {
        vga[0] = (0x0E << 8) | 'K';
    }
    // Se for IRQ0 (timer), mostra 'T' verde
    else if (num == 32) {
        vga[1] = (0x0A << 8) | 'T';
    }
    // Se for exceção, mostra 'E' vermelho + número
    else if (num < 32) {
        vga[2] = (0x0C << 8) | 'E';
        vga[3] = (0x0C << 8) | ('0' + (num % 10));
    }
}
