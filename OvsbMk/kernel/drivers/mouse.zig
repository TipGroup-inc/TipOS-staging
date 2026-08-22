// moe moe kyun <3
// moe moe kyun <3
// Mouse PS/2 driver for TipOS (GPL v2)

// ~~ Estado do mouse ~~
// mouse_cycle: fase de montagem do pacote (0, 1, 2)
// mouse_packet: 3 bytes do pacote PS/2 (flags, dx, dy)
// last_reported_x/y: última posição reportada (pra calcular delta~)
var mouse_cycle: u32 = 0;
var mouse_packet: [3]u8 = undefined;
var last_reported_x: i32 = 512;
var last_reported_y: i32 = 384;

// ~~ MICE_BUF_SIZE ~~
// Buffer circular dos bytes CRUS do mouse PS/2 (pro Xorg via
// /dev/input/mice). O driver mouse do Xorg decodifica o protocolo
// ele mesmo, então a gente só repassa os bytes~
const MICE_BUF_SIZE = 256;
var mice_buf: [MICE_BUF_SIZE]u8 = undefined;
var mice_head: usize = 0;
var mice_tail: usize = 0;

fn mice_push(d: u8) void {
    const next = (mice_head + 1) % MICE_BUF_SIZE;
    if (next != mice_tail) {
        mice_buf[mice_head] = d;
        mice_head = next;
    }
}

fn inb(port: u16) u8 {
    return asm volatile ("inb %[port], %[ret]"
        : [ret] "={al}" (-> u8),
        : [port] "N{dx}" (port),
    );
}

fn outb(port: u16, val: u8) void {
    asm volatile ("outb %[val], %[port]"
        :
        : [port] "N{dx}" (port),
          [val] "{al}" (val),
    );
}

// ~~ mouse_wait ~~
// Espera o controlador PS/2 ficar disponível.
// Se `write` = true, espera o buffer de saída esvaziar (bit 1 da porta 0x64 = 0).
// Se `write` = false, espera o buffer de entrada encher (bit 0 = 1).
// Timeout de 100000 iterações — se passar, desiste (pra não travar~)
fn mouse_wait(write: bool) void {
    var timeout: i32 = 100000;
    while (timeout > 0) {
        if (write) {
            if ((inb(0x64) & 2) == 0) return;
        } else {
            if ((inb(0x64) & 1) != 0) return;
        }
        timeout -= 1;
    }
}

// ~~ mouse_write ~~
// Escreve um comando pro mouse via controlador PS/2.
// Primeiro manda 0xD4 pra porta 0x64 (comando "write aux"),
// depois manda o byte do comando pra porta 0x60.
// Tudo com wait, porque PS/2 é lento e exigente~
fn mouse_write(val: u8) void {
    mouse_wait(true);
    outb(0x64, 0xD4);
    mouse_wait(true);
    outb(0x60, val);
}

// ~~ mouse_read_byte ~~
// Lê um byte de resposta do mouse (via porta 0x60).
// Espera o dado ficar disponível antes de ler~
fn mouse_read_byte() u8 {
    mouse_wait(false);
    return inb(0x60);
}

// ~~ mouse_init ~~
// Inicializa o mouse PS/2!
// Ativa o dispositivo (0xA8), lê o status do controlador,
// seta o bit de IRQ12 (bit 1), escreve de volta,
// manda comando de default (0xF6) e ativa (0xF4).
// Se não chamar isso, o mouse não funciona (lógico~)
export fn mouse_init() void {
    mouse_wait(true);
    outb(0x64, 0xA8);
    mouse_wait(true);
    outb(0x64, 0x20);
    mouse_wait(false);
    const status: u8 = inb(0x60) | 2;
    mouse_wait(true);
    outb(0x64, 0x60);
    mouse_wait(true);
    outb(0x60, status);
    mouse_write(0xF6);
    _ = mouse_read_byte();
    mouse_write(0xF4);
    _ = mouse_read_byte();
}

// ~~ mouse_process_byte ~~
// Processa um byte do mouse. O mouse PS/2 manda pacotes de 3 bytes:
// byte 0: flags (bit 0 = botão esq, bit 1 = dir, bit 2 = meio,
//                bit 4 = sinal X, bit 5 = sinal Y)
// byte 1: delta X (com sinal, estendido se bit 4 de flags = 0)
// byte 2: delta Y (com sinal, estendido se bit 5 de flags = 0)
// Monta o pacote ciclo a ciclo (mouse_cycle 0→1→2→0) e atualiza posição~
// A posição é clampada entre 0..1023 (X) e 0..767 (Y) porque não tenho
// monitor infinito, infelizmente~
export fn mouse_process_byte(d: u8) void {
    mice_push(d);
    switch (mouse_cycle) {
        0 => {
            mouse_packet[0] = d;
            if ((d & 0x08) != 0) {
                mouse_cycle = 1;
            }
        },
        1 => {
            mouse_packet[1] = d;
            mouse_cycle = 2;
        },
        2 => {
            mouse_packet[2] = d;
            mouse_cycle = 0;
            const flags: u8 = mouse_packet[0];
            var dx: i32 = @as(i32, mouse_packet[1]);
            var dy: i32 = @as(i32, mouse_packet[2]);
            if ((flags & 0x10) == 0) dx |= @as(i32, @bitCast(@as(u32, 0xFFFFFF00)));
            if ((flags & 0x20) == 0) dy |= @as(i32, @bitCast(@as(u32, 0xFFFFFF00)));
            mouse_x += dx;
            mouse_y -= dy;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > 1023) mouse_x = 1023;
            if (mouse_y > 767) mouse_y = 767;
            mouse_buttons = @as(i32, flags) & 7;
        },
        else => mouse_cycle = 0,
    }
}

// ~~ mouse_handler ~~
// Handler de IRQ do mouse (IRQ12).
// Lê o byte da porta 0x60 e manda pro processador de pacote.
// Manda EOI pro PIC escravo (0xA0) e pro mestre (0x20).
// PS: o PIC escravo vem primeiro porque ele é mais chato~
export fn mouse_handler() void {
    const data: u8 = inb(0x60);
    mouse_process_byte(data);
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

// ~~ mouse_read_delta ~~
// Retorna o delta (dx, dy) desde a última chamada e o estado dos botões.
// Útil pra interfaces que querem movimento relativo (tipo o compositor~).
// Reseta o "último reportado" pra posição atual depois de ler.
export fn mouse_read_delta(dx: *i32, dy: *i32, buttons: *i32) void {
    dx.* = mouse_x - last_reported_x;
    dy.* = mouse_y - last_reported_y;
    buttons.* = mouse_buttons;
    last_reported_x = mouse_x;
    last_reported_y = mouse_y;
}

// ~~ mice_avail / mice_read ~~
// Bytes crus do mouse pro Xorg (/dev/input/mice). Não bloqueia:
// retorna o que tiver no buffer. O Xorg espera via select()~
export fn mice_avail() i32 {
    if (mice_head != mice_tail) return 1;
    return 0;
}

export fn mice_read() u8 {
    if (mice_head == mice_tail) return 0;
    const b = mice_buf[mice_tail];
    mice_tail = (mice_tail + 1) % MICE_BUF_SIZE;
    return b;
}

// ~~ mouse_x / mouse_y / mouse_buttons ~~
// Posição absoluta do mouse (exportada pra uso externo).
// mouse_buttons: bit 0 = esquerdo, bit 1 = direito, bit 2 = meio.
// Começa no centro da tela (512, 384) porque é fashion~ ☆
export var mouse_x: i32 = 512;
export var mouse_y: i32 = 384;
export var mouse_buttons: i32 = 0;