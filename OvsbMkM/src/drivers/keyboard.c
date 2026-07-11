#include "idt.h"

#define PS2_DATA   0x60
#define KB_BUFFER_SIZE 256

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static volatile char kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;
static volatile int shift_pressed = 0;

// Tabela scancode Set 1 (minúsculas)
static const char sc_ascii_normal[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`', 0,
    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,
    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Tabela shift (maiúsculas e símbolos)
static const char sc_ascii_shift[] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,
    ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

void keyboard_handler(void) {
    uint8_t sc = inb(PS2_DATA);
    
    // Verifica shift (pressionado ou solto)
    if (sc == 0x2A || sc == 0x36) {
        shift_pressed = 1;
        outb(0x20, 0x20);
        return;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shift_pressed = 0;
        outb(0x20, 0x20);
        return;
    }
    
    char c = 0;
    if (!(sc & 0x80) && sc < sizeof(sc_ascii_normal)) {
        c = shift_pressed ? sc_ascii_shift[sc] : sc_ascii_normal[sc];
    }
    
    // Mapeamentos especiais
    if (sc == 0x01) c = 27;  // ESC
    
    if (c) {
        int next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;
        }
    }
    outb(0x20, 0x20);
}

void keyboard_init(void) {
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x02);
}

char keyboard_read(void) {
    while (1) {
        if (kb_head != kb_tail) {
            char c = kb_buffer[kb_tail];
            kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
            return c;
        }
        if (inb(0x64) & 0x01) {
            uint8_t sc = inb(0x60);
            // Verifica shift
            if (sc == 0x2A || sc == 0x36) shift_pressed = 1;
            if (sc == 0xAA || sc == 0xB6) shift_pressed = 0;
            
            char c = 0;
            if (!(sc & 0x80) && sc < sizeof(sc_ascii_normal)) {
                c = shift_pressed ? sc_ascii_shift[sc] : sc_ascii_normal[sc];
            }
            if (sc == 0x01) c = 27;
            if (c) return c;
        }
        for (volatile int i = 0; i < 1000; i++);
    }
}