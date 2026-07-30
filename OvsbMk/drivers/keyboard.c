/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: keyboard.c ~ funcoes anotadas: 11
 */
/* ♥ keyboard.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ KEYBOARD ~ driver de teclado! digita que eu leio~ */
/* ♥ KEYBOARD - Driver de Teclado PS/2 ~ "Aperte qualquer tecla~"
 * Dica: scancodes set 1 traduzidos pra ASCII direto~
 * Se apertar e não aparecer nada, é porque o buffer encheu~ kyun! */
#include "idt.h"

#define PS2_DATA   0x60
#define PS2_CTRL   0x64
#define KB_BUFFER_SIZE 256

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ~ essa demorou pra debugar, respeita ~ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static volatile char kb_buffer[KB_BUFFER_SIZE];
extern volatile int sigint_pending;
static volatile int kb_head = 0;
static volatile int kb_tail = 0;
volatile int shift_pressed = 0;
volatile int ctrl_pressed = 0;
static volatile int ext_flag = 0;

// ─── Keyboard repeat ────────────────────────────────────
static volatile int last_make_sc = 0;
static volatile char last_repeat_char = 0;
static volatile uint64_t press_tick = 0;
static volatile uint64_t last_repeat_tick = 0;
extern volatile uint64_t timer_ticks;

#define REPEAT_DELAY  80
#define REPEAT_RATE    8

static const char norm[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,
    ' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static const char shf[] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',0,
    '|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,
    ' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

struct { uint8_t sc; const char *seq; int len; } ext[] = {
    {0x48,"\x1b[A",3},{0x50,"\x1b[B",3},{0x4B,"\x1b[D",3},{0x4D,"\x1b[C",3},
    {0x47,"\x1b[H",3},{0x4F,"\x1b[F",3},{0x49,"\x1b[5~",4},{0x51,"\x1b[6~",4},
    {0x52,"\x1b[2~",4},{0x53,"\x1b[3~",4},
    {0x3B,"\x1bOP",3},{0x3C,"\x1bOQ",3},{0x3D,"\x1bOR",3},{0x3E,"\x1bOS",3},
    {0x3F,"\x1b[15~",5},{0x40,"\x1b[17~",5},{0x41,"\x1b[18~",5},{0x42,"\x1b[19~",5},
    {0x43,"\x1b[20~",5},{0x44,"\x1b[21~",5},{0x57,"\x1b[23~",5},{0x58,"\x1b[24~",5},
};

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void kbuf_put(char c) {
    int n = (kb_head + 1) % KB_BUFFER_SIZE;
    if (n != kb_tail) { kb_buffer[kb_head] = c; kb_head = n; }
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void kbuf_put_seq(const char *s, int len) {
    for (int i = 0; i < len; i++) kbuf_put(s[i]);
}

/* ~ cuidado que essa aqui morde ~ */
static char scancode_to_ascii(uint8_t sc) {
    if (sc >= sizeof(norm)) return 0;
    return shift_pressed ? shf[sc] : norm[sc];
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int handle_extended(uint8_t sc) {
    for (int i = 0; i < (int)(sizeof(ext)/sizeof(ext[0])); i++) {
        if (sc == ext[i].sc) { kbuf_put_seq(ext[i].seq, ext[i].len); return 1; }
    }
    if (sc == 0x1D) { ctrl_pressed = 1; return 1; }
    if (sc == 0x9D) { ctrl_pressed = 0; return 1; }
    if (sc == 0x38) { shift_pressed = 2; return 1; }
    if (sc == 0xB8) { shift_pressed = 0; return 1; }
    if (sc == 0x35) { kbuf_put('/'); return 1; }
    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int process_scancode(uint8_t sc) {
    if (sc == 0xE0) { ext_flag = 1; return 1; }
    if (sc == 0xE1) { ext_flag = 1; return 1; }
    if (ext_flag) { ext_flag = 0; return handle_extended(sc); }
    // shift keys
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 1; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 1; }
    if (sc == 0x1D) { ctrl_pressed = 1; return 1; }
    if (sc == 0x9D) { ctrl_pressed = 0; return 1; }
    if (sc == 0x38) { shift_pressed = 1; return 1; }
    if (sc == 0xB8) { shift_pressed = 0; return 1; }
    // break code → key released
    if (sc & 0x80) {
        uint8_t make = sc & 0x7F;
        if (make == last_make_sc) {
            last_make_sc = 0;
            last_repeat_char = 0;
        }
        return 1;
    }
    // make code
    if (sc < sizeof(norm)) {
        char c = scancode_to_ascii(sc);
        if (ctrl_pressed && c >= 'a' && c <= 'z') c = c - 'a' + 1;
        if (ctrl_pressed && c >= 'A' && c <= 'Z') c = c - 'A' + 1;
        if (c) {
            kbuf_put(c);
            last_make_sc = sc;
            last_repeat_char = c;
            press_tick = timer_ticks;
            last_repeat_tick = timer_ticks;
            return 1;
        }
    }
    return 0;
}

/* ~~ Vai tratar esse evento~ se prepara pro caos! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void keyboard_handler(void) {
    uint8_t sc = inb(PS2_DATA);
    process_scancode(sc);
    outb(0x20, 0x20);
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void keyboard_init(void) {
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x02);
}

/* ~~ Lendo coisinhas~ toma cuidado pra não ler lixo! */
/* ~ cuidado que essa aqui morde ~ */
char keyboard_read(void) {
    while (1) {
        if (kb_head != kb_tail) {
            char c = kb_buffer[kb_tail];
            kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
            if (c == 3) { sigint_pending = 1; }
            last_make_sc = 0;
            last_repeat_char = 0;
            return c;
        }
        if (last_make_sc && last_repeat_char) {
            uint64_t held = timer_ticks - press_tick;
            if (held > REPEAT_DELAY && (timer_ticks - last_repeat_tick) >= REPEAT_RATE) {
                last_repeat_tick = timer_ticks;
                return last_repeat_char;
            }
        }
        for (volatile int i = 0; i < 500; i++);
    }
}

/* ~~ keyboard_avail ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int keyboard_avail(void) {
    return (kb_head != kb_tail) ? 1 : 0;
}




/* ♥ keyboard.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
