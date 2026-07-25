/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: mouse.c ~ funcoes anotadas: 9
 */
/* ~*~ mouse.c — Driver de Mouse PS/2~*~ */
/* "Clique aqui, clique ali, oh mouse maroto!"
   Pacotes de 3 bytes do mouse: dx, dy, botoes~
   Se o mouse nao se mexer, e porque nao inicializou~ baka! <3 */
#include <stdint.h>
#include "mouse.h"

/* Posicao atual do mouse ~ comeca no centro da tela (512, 384) */
int mouse_x = 512, mouse_y = 384;
int mouse_buttons = 0;  /* Estado dos botoes: bit 0 = esquerdo, bit 1 = direito */
static int mouse_cycle = 0;     /* Ciclo de leitura do pacote de 3 bytes */
static uint8_t mouse_packet[3]; /* Buffer do pacote PS/2 do rato */

/* APENAS posicao absoluta - sem acumuladores ~ simplificou, sua preguiçosa! */
static int last_reported_x = 512, last_reported_y = 384;

/* Inline assembly pra ler/escrever portas I/O ~ a magica do hardware! */
static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1, %0":"=a"(r):"Nd"(p)); return r; }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile ("outb %0, %1"::"a"(v),"Nd"(p)); }

/* mouse_process_byte: processa UM byte do mouse PS/2 ~
   Junta 3 bytes pra formar um pacote: status, dx, dy */
/* ~ essa demorou pra debugar, respeita ~ */
void mouse_process_byte(uint8_t d) {
    switch(mouse_cycle) {
        case 0: if(!(d&0x08)) return; mouse_packet[0]=d; mouse_cycle++; break;
        case 1: mouse_packet[1]=d; mouse_cycle++; break;
        case 2:
            mouse_packet[2]=d; mouse_cycle=0;
            mouse_buttons = mouse_packet[0] & 0x07;
            int dx = mouse_packet[1], dy = mouse_packet[2];
            if(mouse_packet[0] & 0x10) dx -= 256;
            if(mouse_packet[0] & 0x20) dy -= 256;
            if(dx != 0 || dy != 0) {
                mouse_x += dx;
                mouse_y -= dy;
            }
            break;
    }
}

/* Getters simples ~ "cadê o rato?" */
int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
int mouse_get_buttons(void) { return mouse_buttons; }

/* NOVO: retorna delta desde a última chamada ~ "quanto andou?" */
int mouse_get_dx(void) { int d = mouse_x - last_reported_x; last_reported_x = mouse_x; return d; }
int mouse_get_dy(void) { int d = mouse_y - last_reported_y; last_reported_y = mouse_y; return d; }

/* NOVO: retorna 1 se a posição mudou desde a última chamada */
int mouse_has_moved(void) { return (mouse_x != last_reported_x || mouse_y != last_reported_y); }

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int mouse_wait_write(void) {
    for (int t = 0; t < 100000; t++) {
        if (!(inb(0x64) & 2)) return 0;
    }
    return -1;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int mouse_wait_read(void) {
    for (int t = 0; t < 100000; t++) {
        if (inb(0x64) & 1) return 0;
    }
    return -1;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void mouse_send_cmd(uint8_t cmd) {
    mouse_wait_write();
    outb(0x64, cmd);
}

/* ~ essa demorou pra debugar, respeita ~ */
static void mouse_send_byte(uint8_t b) {
    mouse_wait_write();
    outb(0x60, b);
}

/* ~ cuidado que essa aqui morde ~ */
static uint8_t mouse_read_byte(void) {
    mouse_wait_read();
    return inb(0x60);
}

/* mouse_init: inicializa o mouse PS/2 ~~
   Sequência padrão i8042 + enable IRQ12 no PIC slave.
   Com timeouts pra não travar em hardware sem mouse. */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void mouse_init(void) {
    /* 1. Enable auxiliary port */
    mouse_send_cmd(0xA8);

    /* 2. Read command byte, set bit 1 (IRQ12 enable), write back */
    mouse_send_cmd(0x20);
    uint8_t cmd = mouse_read_byte();
    cmd |= 2;    /* enable IRQ12 */
    cmd &= ~0x20; /* enable mouse clock */
    mouse_send_cmd(0x60);
    mouse_send_byte(cmd);

    /* 3. Set defaults on mouse */
    mouse_send_cmd(0xD4);
    mouse_send_byte(0xF6);
    uint8_t ack = mouse_read_byte();

    /* 4. Enable data reporting */
    mouse_send_cmd(0xD4);
    mouse_send_byte(0xF4);
    ack = mouse_read_byte();

    /* 5. Unmask IRQ12 on PIC slave */
    uint8_t slave_mask = inb(0xA1);
    outb(0xA1, slave_mask & ~0x10);
}

/* mouse_handler: chamado pela IRQ12 — lê porta 0x60 e processa ~~
   Manda EOI pros dois PICs porque IRQ12 fica no slave ~~ */
/* mouse_read_delta: le delta X/Y e estado dos botoes de uma vez ~~
   Usado pela syscall 204 pra userspace (disp compositor) ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void mouse_read_delta(int *dx, int *dy, int *buttons) {
    if (dx)      *dx      = mouse_get_dx();
    if (dy)      *dy      = mouse_get_dy();
    if (buttons) *buttons = mouse_get_buttons();
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void mouse_handler(void) {
    uint8_t data = inb(0x60);
    mouse_process_byte(data);
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* ♥ mouse.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
