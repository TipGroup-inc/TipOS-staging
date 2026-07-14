#include <stdint.h>
#include "mouse.h"

static int mouse_x = 512, mouse_y = 384;
static int mouse_buttons = 0;
static int mouse_cycle = 0;
static uint8_t mouse_packet[3];

/* APENAS posição absoluta - sem acumuladores */
static int last_reported_x = 512, last_reported_y = 384;

static inline uint8_t inb(uint16_t p) { uint8_t r; __asm__ volatile ("inb %1, %0":"=a"(r):"Nd"(p)); return r; }
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile ("outb %0, %1"::"a"(v),"Nd"(p)); }

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

int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
int mouse_get_buttons(void) { return mouse_buttons; }

/* NOVO: retorna delta desde a última chamada */
int mouse_get_dx(void) { int d = mouse_x - last_reported_x; last_reported_x = mouse_x; return d; }
int mouse_get_dy(void) { int d = mouse_y - last_reported_y; last_reported_y = mouse_y; return d; }

/* NOVO: retorna 1 se a posição mudou desde a última chamada */
int mouse_has_moved(void) { return (mouse_x != last_reported_x || mouse_y != last_reported_y); }

void mouse_init(void) {
    outb(0x64,0xA8); outb(0x64,0x20); uint8_t c=inb(0x60);
    outb(0x64,0x60); outb(0x60,(c|0x02)&~0x20);
    outb(0x64,0xD4); outb(0x60,0xF6); inb(0x60);
    outb(0x64,0xD4); outb(0x60,0xF4); inb(0x60);
    outb(0xA1,inb(0xA1)&~0x10);
}

void mouse_handler(void) {
    inb(0x60);
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
