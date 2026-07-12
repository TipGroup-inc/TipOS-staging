# TipOS Kernel — Documentação Completa

> **Nível:** Bizarro. Até uma pedra consegue continuar o desenvolvimento.
>
> **Arquitetura:** x86-64, long mode, ring 0, GRUB/Multiboot2, single-address-space.
> **Linguagens:** C11 (GCC) + NASM (assembly).
> **Build:** Makefile + GRUB + QEMU.

---

## Índice

1. [Arquitetura Geral](#1-arquitetura-geral)
2. [Boot Step-by-Step](#2-boot-step-by-step)
3. [Layout de Memória](#3-layout-de-memória)
4. [IDT — Interrupt Descriptor Table](#4-idt)
5. [Syscalls](#5-syscalls)
6. [VGA Text Mode + ANSI Parser](#6-vga)
7. [Teclado PS/2](#7-teclado)
8. [ATA PIO](#8-ata)
9. [FAT32](#9-fat32)
10. [PIT — Timer Programável](#10-pit)
11. [RTC — Relógio de Tempo Real](#11-rtc)
12. [Shell](#12-shell)
13. [Redirecionamento e Pipes](#13-redirecionamento)
14. [Comandos](#14-comandos)
15. [Userland e Libc](#15-userland)
16. [Graphy — Editor TUI](#16-graphy)
17. [Como Adicionar um Syscall](#17-como-adicionar-uma-syscall)
18. [Como Adicionar um Comando](#18-como-adicionar-um-comando)
19. [Como Adicionar uma Função Libc](#19-como-adicionar-uma-função-libc)
20. [Debugging](#20-debugging)
21. [Problemas Comuns](#21-problemas-comuns)

---

## 1. Arquitetura Geral

```
                    ┌─────────────────────────────────────────┐
                    │             USERLAND (ring 0)           │
                    │   graphy (editor)  |  bash (embedded)   │
                    │   ls (embedded)    |  dyld (embedded)   │
                    └──────────────┬──────────────────────────┘
                                   │ int 0x80 (XNU convention)
                    ┌──────────────┴──────────────────────────┐
                    │             KERNEL (ring 0)             │
                    │   syscall dispatcher → 30 syscalls     │
                    │   VGA (ANSI) | Keyboard | ATA | FAT32  │
                    │   Shell (history, line edit, PATH, >)   │
                    │   PIT timer | RTC clock | Compositor   │
                    └─────────────────────────────────────────┘
```

**Características:**
- **Monolítico** — tudo no kernel (drivers, FS, shell inline).
- **Single-address-space** — kernel e userland compartilham o mesmo espaço de endereçamento. Não há separação ring 3 / proteção de página.
- **Execução de binários:** Mach-O 64-bit (`LC_SEGMENT_64` + `LC_MAIN`), carregado via `mach_o_load()`.
- **Interrupções:** IDT com 32 ISRs (exceções) + IRQ0 (timer) + IRQ1 (teclado) + int 0x80 (syscall).

---

## 2. Boot Step-by-Step

### 2.1 GRUB → boot64.asm

```
MBR → GRUB stage 1+2 → kernel.elf (multiboot2) → boot64.asm → kmain()
```

1. **GRUB** carrega `kernel.elf` via Multiboot2 (grub.cfg: `multiboot2 /boot/kernel.elf`).
2. **boot64.asm** (`OvsbMkM/src/kernel/boot64.asm`):
   - Recebe controle em 32-bit protected mode.
   - Configura **PML4 → PDP → PD** com páginas de 2MB (identity mapping dos primeiros 1GB).
   - Habilita **PAE**, **long mode** (EFER.LME = 1), **paging** (CR0.PG = 1).
   - Carrega **GDT** (64-bit CS/DS).
   - Salta via `far jmp` para código 64-bit.
   - Inicializa stack (0x90000), zera BSS, chama `kmain()`.

### 2.2 kmain() — Sequência de Inicialização

```c
void kmain(void) {
    idt_init();         // 1. Configura IDT (exceções, IRQs, handlers)
    pic_init();         // 2. PIC 8259 — remapeia IRQs 0-15 para vetores 32-47
    idt_set_syscall();  // 3. Registra int 0x80 como syscall gate (DPL=3)
    idt_set_irq1();     // 4. Registra handler customizado para IRQ1 (teclado)
    keyboard_init();    // 5. Habilita IRQ1 no PIC
    pit_init();         // 6. Programa PIT para 100 Hz
    memory_init();      // 7. Inicializa heap (bump allocator 4MB @ 0x900000)
    __asm__ ("sti");    // 8. Habilita interrupções
    smc_init();         // 9. SMC stub
    nvram_init();       // 10. NVRAM stub
    serial_init();      // 11. COM1 serial (debug logging)
    ata_init();         // 12. ATA PIO init (primary master)
    fat32_init();       // 13. Lê BPB, monta FAT32
    vga_puts("...\n");  // 14. Mensagens de boot
    shell_loop();       // 15. → SHELL (nunca retorna)
}
```

**Ordem importa:** PIC antes de habilitar IRQs, ATA antes de FAT32, FAT32 antes de shell.

---

## 3. Layout de Memória

```
Endereço       | Conteúdo
───────────────┼──────────────────────────────────────────
0x000000       | IVT real-mode (não usado)
0x0007C00      | Boot sector (não usado após GRUB)
0x00100000     | Kernel ELF carregado aqui (1MB+)
0x00200000     | Page tables: PML4, PDP, PD (2MB pages)
0x00900000     | Stack do kernel
0x80000000     | Heap do kernel (bump allocator, 4MB)
0x90000000     | (fim do identity mapping de 1GB)
0xB8000        | VGA text mode buffer (80×25)
───────────────┼──────────────────────────────────────────
0x02000000     | Userland programas carregados aqui
```

### Page Tables (boot64.asm)

```
PML4[0] → PDP[0] → PD[0..511] → cada PD[i] mapeia 2MB
Total: PML4[0] cobre 0x0 – 0x40000000 (1GB) em páginas de 2MB
```

**Não há** paginação por processo. Todo o kernel e userland compartilham o mesmo espaço.

### Heap do Kernel (memory.c)

```
Região:  0x800000 – 0xC00000 (4MB)
kmalloc(): bump allocator, O(1), sem kfree() para bump.
kfree(): libera apenas se for alocado via page allocator (mmap_user).
Page allocator: bitmap de 1024 páginas de 4096 bytes.
```

---

## 4. IDT

### 4.1 Estrutura (idt.h)

```c
typedef struct {
    uint16_t offset_low;    // bits 0-15 do handler
    uint16_t selector;      // seletor GDT (0x08 = kernel code)
    uint8_t  ist;           // Interrupt Stack Table (0 = default)
    uint8_t  type_attr;     // tipo + flags
    uint16_t offset_mid;    // bits 16-31
    uint32_t offset_high;   // bits 32-63
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;
```

### 4.2 Mapa de Vetores

| Vetor | Uso | Handler |
|-------|-----|---------|
| 0-31 | Exceções CPU | `isr0()`–`isr31()` em idt.asm → `idt_handler()` |
| 32 (IRQ0) | Timer PIT | `irq0` em idt.asm → `timer_tick_handler()` |
| 33 (IRQ1) | Teclado PS/2 | `keyboard_irq_handler` em keyboard_asm.asm |
| 0x80 (128) | Syscall | `syscall_handler_entry` em syscall_entry.asm |

### 4.3 Fluxo de Interrupção

Para exceções (0-31) e timer original:

```
CPU → IDT entry → idt.asm handler → push error code + vector num
                                    → push all regs
                                    → call idt_handler(num, err)
                                    → restore all regs
                                    → iretq
```

Para teclado (IRQ1):

```
CPU → IDT entry (keyboard_irq_handler)
    → keyboard_asm.asm: push all regs → call keyboard_handler() → EOI → pop → iretq
```

Para timer (IRQ0) — NOVO handler:

```
CPU → IDT entry (irq0)
    → idt.asm: push regs → call timer_tick_handler() → EOI → pop → iretq
```

### 4.4 Adicionar Novo Handler

1. Em `idt.asm`, crie o entry point (push regs, call C function, pop, iretq).
2. Em `idt.c`, registre com `idt_set_entry(vetor, handler, 0x08, IDT_PRESENT | IDT_INT_GATE)`.
3. Em `idt.h`, declare o vetor se necessário.

---

## 5. Syscalls

### 5.1 Convenção (XNU)

```
Entrada:  RAX = syscall number, RDI = a1, RSI = a2, RDX = a3, RCX = a4
Saída:    RAX = valor de retorno
Gate:     int 0x80 (DPL=3, chamável de userland)
```

### 5.2 Tabela de Syscalls

| # | Nome | Descrição | Arquivo (linha) |
|---|------|-----------|-----------------|
| 1 | `exit` | Termina processo | syscall.c:75 |
| 3 | `read` | Ler de fd 0 (teclado) ou arquivo | syscall.c:92 |
| 4 | `write` | Escrever em fd 1/2 (VGA) ou arquivo | syscall.c:77 |
| 5 | `open` | Abre arquivo ou /dev/* | syscall.c:117 |
| 6 | `close` | Fecha fd | syscall.c:131 |
| 10 | `unlink` | Remove arquivo | syscall.c:181 |
| 20 | `getpid` | Retorna 1 (stub) | syscall.c:198 |
| 24 | `getuid` | Retorna 0 | syscall.c:201 |
| 25 | `geteuid` | Retorna 0 | syscall.c:201 |
| 33 | `access` | Verifica acesso (stub) | syscall.c:134 |
| 47 | `getgid` | Retorna 0 | syscall.c:203 |
| 48 | `getegid` | Retorna 0 | syscall.c:203 |
| 54 | `ioctl` | Stub | syscall.c:205 |
| 73 | `munmap` | Libera páginas | syscall.c:191 |
| 74 | `mprotect` | Stub | syscall.c:194 |
| 116 | `gettimeofday` | Relógio real (RTC) | syscall.c:210 |
| 134 | `sigaction` | Stub | syscall.c:207 |
| 136 | `mkdir2` | Cria diretório FAT32 | syscall.c:184 |
| 137 | `rmdir2` | Remove diretório FAT32 | syscall.c:187 |
| 173 | `sigreturn` | Stub | syscall.c:208 |
| 188 | `stat` | Info arquivo | syscall.c:151 |
| 189 | `fstat` | Info por fd | syscall.c:140 |
| 197 | `mmap` | Aloca páginas anônimas | syscall.c:190 |
| 198 | `kbhit` | Verifica tecla disponível | syscall.c:196 |
| 199 | `lstat` | Idêntico a stat | syscall.c:151 |
| 200 | `lseek` | Posiciona em arquivo | syscall.c:163 |

### 5.3 Fluxo de Syscall

```
Userland: RAX=num, RDI=a1, RSI=a2, RDX=a3 → int 0x80
  ↓
syscall_entry.asm: salva regs, rearranja args para C calling convention
  ↓
syscall_handler(num, a1, a2, a3, a4) → switch(num) → executa handler
  ↓
syscall_entry.asm: restaura regs → iretq → retorna para userland
```

### 5.4 gettimeofday — Implementação Real

```c
case SYS_gettimeofday: {
    struct timeval *tv = (struct timeval *)a1;
    if (tv) {
        rtc_time t;
        rtc_read(&t);
        // converter data/hora para epoch (Unix timestamp)
        // algoritmo simples: dias desde 2000-01-01 + segundos do dia
        static const int days_per_mon[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int y = t.yr - 2000;
        int days = y * 365 + (y + 3) / 4; // anos bissextos aproximados
        for (int i = 0; i < t.mo - 1; i++) days += days_per_mon[i];
        if (t.mo > 2 && ((t.yr % 4 == 0 && t.yr % 100 != 0) || t.yr % 400 == 0)) days++;
        days += t.dy - 1;
        tv->tv_sec = (uint64_t)days * 86400 + t.h * 3600 + t.m * 60 + t.s;
        tv->tv_usec = 0;
    }
    return 0;
}
```

---

## 6. VGA

### 6.1 Modo Texto 80x25

- Buffer: `0xB8000`, 80×25 = 2000 células de 2 bytes cada.
- Cada célula: `byte baixo = caractere ASCII`, `byte alto = atributo de cor (foreground << 4 | background)`.
- Cor padrão: `0x0A` (verde claro em fundo preto).
- Reverse video: `0x70` (preto em fundo branco).

### 6.2 Funções Principais

```c
void vga_putchar(char c);   // Escreve 1 caractere com parsing ANSI
void vga_puts(const char *s); // Escreve string
void vga_clear(void);        // Limpa tela, reseta cursor e estado ANSI
void vga_putchar(char c);    // Gerencia scroll, cursor, e ANSI
```

### 6.3 ANSI Escape Parser

O parser está em `vga_putchar()` (kernel.c). Estados:

```
ESC (0x1B) → estado 1
  '[' → estado 2: acumula parâmetros (CSI)
    dígitos → acumula em esc_params[esc_np]
    ';' → avança esc_np
    '?' → seta flag esc_question (para sequências com ?)
    letra → executa comando:
      'H'/'f' → cursor position (row=params[0]-1, col=params[1]-1)
      'J' → clear screen (params[0]==2)
      'K' → clear to end of line
      'm' → SGR: 7=reverse, 0=reset
      'l' (com ?25) → show cursor
      'h' (com ?25) → hide cursor
  'A'/'B'/'C'/'D' → seta (up/down/right/left) — sem parâmetros
  'H' → home (0,0)
```

**Exemplo — `\x1b[5;10H` posiciona cursor na linha 5, coluna 10:**
```
vga_putchar('\x1b') → esc_state=1
vga_putchar('[')    → esc_state=2, esc_np=0, esc_params[0]=0
vga_putchar('5')    → esc_params[0]=5
vga_putchar(';')    → esc_np=1, esc_params[1]=0
vga_putchar('1')    → esc_params[1]=1
vga_putchar('0')    → esc_params[1]=10
vga_putchar('H')    → executa: cy=5-1=4, cx=10-1=9
```

### 6.4 Variáveis de Estado do Parser

```c
static int esc_state;           // 0=normal, 1=ESC recebido, 2=CSI, 3=CSI params
static int esc_params[4];       // parâmetros numéricos
static int esc_np;              // índice do parâmetro atual
static int esc_question;        // flag para sequências com '?'
static int esc_rev;             // 1 = reverse video ativo
static int cur_visible;         // 1 = cursor do hardware visível
```

### 6.5 Controle de Cursor do Hardware

Portas VGA: `0x3D4` (index), `0x3D5` (data)
- Cursor position: index 0x0F (low byte), 0x0E (high byte)
- `pos = cy * 80 + cx`

---

## 7. Teclado

### 7.1 Hardware

- Controlador PS/2, porta de dados `0x60`, porta de status `0x64`.
- IRQ1 (vetor 33) dispara quando uma tecla é pressionada/solta.

### 7.2 Keyboard Handler (keyboard_asm.asm + keyboard.c)

```
IRQ1 → keyboard_irq_handler (asm) → keyboard_handler() (C)
  → lê scancode de 0x60
  → process_scancode() → traduz para ASCII ou sequência VT100
  → coloca no buffer circular kb_buffer[256]
  → EOI (0x20 para 0x20)
  → iretq
```

### 7.3 Scancode → ASCII (Set 1)

Array `norm[]` (sem shift) e `shf[]` (com shift):
- Scancode é usado como índice no array.
- `0x2A` e `0x36` = shift pressionado, `0xAA` e `0xB6` = shift solto.
- Teclas estendidas (prefixo `0xE0`): setas, Home/End, etc. → emitem sequências VT100 (`\x1b[A`, etc.).

### 7.4 Keyboard Repeat System

```c
// Variáveis de estado
static volatile int last_make_sc;       // último scancode pressionado
static volatile char last_repeat_char;  // caractere a repetir
static volatile uint64_t press_tick;    // timer_ticks quando foi pressionado
static volatile uint64_t last_repeat_tick; // timer_ticks do último repeat emitido

// Em process_scancode():
//   make code (sc & 0x80 == 0): last_make_sc = sc, last_repeat_char = c, press_tick = timer_ticks
//   break code (sc & 0x80): last_make_sc = 0, last_repeat_char = 0

// Em keyboard_read():
//   se buffer vazio e last_make_sc != 0:
//     held = timer_ticks - press_tick
//     se held > REPEAT_DELAY (800ms) e (timer_ticks - last_repeat_tick) >= REPEAT_RATE (12.5Hz):
//       last_repeat_tick = timer_ticks
//       return last_repeat_char
```

**Para ajustar:** mude `REPEAT_DELAY` (ticks) e `REPEAT_RATE` (tick interval) em `keyboard.c`.

### 7.5 keyboard_read() — Bloqueante (buffer-only)

```c
char keyboard_read(void) {
    while (1) {
        if (kb_head != kb_tail)     // caractere no buffer?
            return kb_buffer[...];  // sim, retorna (reseta repeat state)
        if (repeat_logic)           // repetir tecla?
            return last_repeat_char;
        delay(500);                 // polling delay
    }
}
```

**Nota:** A leitura do hardware PS/2 (`0x60`) é feita EXCLUSIVAMENTE pelo ISR (`keyboard_handler`). O `keyboard_read()` NÃO faz polling direto da porta para evitar duplicação de caracteres causada pela disputa ISR × polling.

**Histórico:** Anteriormente, `keyboard_read()` fazia polling da porta `0x60` quando o buffer estava vazio. Isso causava duplicação de caracteres porque tanto o ISR quanto o polling liam o mesmo scancode. A solução foi remover o polling e deixar apenas o ISR como fonte de dados.

---

## 8. ATA

### 8.1 Driver PIO (ata.c)

- **Portas:** 0x1F0–0x1F7 (primary IDE, master).
- **LBA28:** setor de 512 bytes, endereçamento linear de 28 bits.
- **Operações:**
  - `ata_read_sector(uint32_t lba, uint8_t *buf)` — espera DRQ, lê 256 words.
  - `ata_write_sector(uint32_t lba, uint8_t *buf)` — espera DRQ, escreve 256 words.
- **Init:** `ata_init()` — espera 1 segundo (busy loop), seleciona drive 0.

### 8.2 Curiosidade GCC 15+

O compilador GCC 15+ gera `inw %edx` em vez de `inw %dx`. O Makefile aplica um `sed` para corrigir o assembly gerado por GCC:

```makefile
OvsbMkM/build/src/drivers/ata.o: OvsbMkM/src/drivers/ata.c
    $(CC) $(CFLAGS) -S $< -o $*.s
    sed -i 's/inw %edx/inw %dx/g; s/outw %edx/outw %dx/g' $*.s
    as --64 $*.s -o $@
```

---

## 9. FAT32

### 9.1 Inicialização (fat32_init)

```c
int fat32_init(void) {
    // Lê setor 0 (boot sector / VBR)
    // Extrai: bytes_per_sector, sectors_per_cluster, reserved_sectors,
    //         fat_count, fat_size (sectors), root_cluster
    // Guarda em globais
    // Retorna 0 em caso de sucesso
}
```

### 9.2 API Completa

| Função | Descrição | Código de erro |
|--------|-----------|----------------|
| `fat32_read_file(name, buf, max)` | Lê arquivo | bytes lidos ou <0 |
| `fat32_write_file(name, buf, size)` | Escreve (sobrescreve) | bytes ou <0 |
| `fat32_create_file(name)` | Cria arquivo vazio | 0 ou FAT_ERR_* |
| `fat32_delete_file(name)` | Remove arquivo | 0 ou <0 |
| `fat32_list_dir()` | Lista diretório atual na VGA | num entries |
| `fat32_change_dir(name)` | Muda diretório | 0 ou <0 |
| `fat32_get_cwd_name(buf, max)` | Caminho completo | void |
| `fat32_mkdir(name)` | Cria diretório | 0 ou <0 |
| `fat32_rmdir(name)` | Remove diretório vazio | 0 ou <0 |
| `fat32_rename(old, new)` | Renomeia | 0 ou <0 |
| `fat32_stat(name, &size, &attr)` | Info arquivo | 0 ou <0 |

### 9.3 Arquitetura Interna

```
fat32_boot_t (boot sector struct) → lê do setor 0
  → cluster_to_sector(n) → (n - 2) * sectors_per_cluster + data_start
  → find_entry(dir_cluster, name) → busca no diretório
  → fat_read_entry(n) → lê FAT entry (próximo cluster na chain)
  → fat_write_entry(n, val) → escreve FAT entry
  → read_chain(cluster, buf, size) → percorre chain de clusters, lê dados
  → write_chain(cluster, buf, size) → aloca clusters, escreve dados
  → fat_find_free() → encontra cluster livre na FAT
```

### 9.4 Nomes de Arquivo

- Conversão para 8.3 (uppercase, remove extensão longa):
  - `name_to_83("hello.c", out)` → `"HELLO   C  "` (8+3, padding com espaços)
  - `name_to_83("Makefile", out)` → `"MAKEFILE   "` (sem extensão)
  - `name_to_83("file.txt.bak", out)` → `"FILE~1  BAK"` (8.3 truncado)

### 9.5 Estrutura de Diretório no Disco

```
TipOS usa disk.img de 64MB FAT32.
Partições:
  /BIN/       → binários Mach-O (GRAPHY, etc)
  /USR/BIN/   → PATH secundário (futuro)
  /LOCAL/BIN/ → PATH terciário
```

---

## 10. PIT

### 10.1 Inicialização (pit_init)

```c
static void pit_init(void) {
    uint32_t div = 1193182 / 100; // 100 Hz (frequência base do PIT = 1.193182 MHz)
    outb(0x43, 0x36);             // Canal 0, lobyte/hibyte, modo 3 (square wave)
    outb(0x40, div & 0xFF);       // Low byte do divisor
    outb(0x40, (div >> 8) & 0xFF); // High byte
}
```

### 10.2 Timer Tick Handler

```c
volatile uint64_t timer_ticks = 0;  // incrementado a cada IRQ0 (~100Hz)

void timer_tick_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20); // EOI — avisa PIC que IRQ0 foi tratada
}
```

Chamado pelo entry point `irq0` em `idt.asm`, que salva registradores, chama `timer_tick_handler()`, restaura e executa `iretq`.

### 10.3 Funções Utilitárias

```c
static void sleep_ms(uint64_t ms) {
    uint64_t target = timer_ticks + ms / 10 + 1; // +1 para arredondar
    while (timer_ticks < target) { __asm__ volatile ("pause"); }
}
```

**Atenção:** `sleep_ms` é busy-wait. Não há scheduler, então a CPU fica 100% ocupada durante o sleep.

---

## 11. RTC

### 11.1 CMOS Ports

```
Porta 0x70: índice do registrador CMOS
Porta 0x71: dado lido/escrito
Bit 7 de 0x70: NMI (Non-Maskable Interrupt) — deve ser preservado
```

**Registradores da data/hora:**

| Registro | Campo | Formato |
|----------|-------|---------|
| 0x00 | Segundo | BCD |
| 0x02 | Minuto | BCD |
| 0x04 | Hora | BCD |
| 0x07 | Dia do mês | BCD |
| 0x08 | Mês | BCD |
| 0x09 | Ano (0-99) | BCD |
| 0x0B | Status B | bit 2=0 → BCD, bit 2=1 → binary |

### 11.2 Leitura (rtc_read)

```c
static uint8_t read_cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int cmos_bcd(int v) {
    return (v & 0x0F) + ((v / 16) * 10); // converte BCD para int
}

void rtc_read(rtc_time *t) {
    t->s  = cmos_bcd(read_cmos(0x00));
    t->m  = cmos_bcd(read_cmos(0x02));
    t->h  = cmos_bcd(read_cmos(0x04));
    t->dy = cmos_bcd(read_cmos(0x07));
    t->mo = cmos_bcd(read_cmos(0x08));
    t->yr = cmos_bcd(read_cmos(0x09)) + 2000;
}
```

### 11.3 Comandos `date` e `uptime`

`date` → lê RTC, formata `YYYY-MM-DD HH:MM:SS`.
`uptime` → lê `timer_ticks / 100`, formata `Xd HH:MM`.

---

## 12. Shell

### 12.1 Arquitetura

O shell roda inline no kernel, dentro de `shell_loop()` (kernel.c). É um loop infinito que lê teclas e executa comandos.

```
shell_loop():
  1. Mostra prompt "MkM> "
  2. Lê tecla (read_key())
  3. Se Enter → executa comando
  4. Se ↑/↓ → navega history
  5. Se ←/→/Home/End → move cursor na linha
  6. Se Ctrl+* → ações especiais (^A home, ^E end, ^K kill, etc)
  7. Se printável → insere no buffer
  8. Volta para 2
```

### 12.2 Shell History

```c
#define HIST_MAX 128
#define CMD_MAX  256
static char history[HIST_MAX][CMD_MAX];
static int hist_count = 0;
```

- **Circular:** quando `hist_count` > `HIST_MAX`, sobrescreve o mais antigo com `hist_count % HIST_MAX`.
- **Navegação:** `browse_idx` rastreia posição atual no history. `-1` = não navegando (linha original).
- **↑:** ativa navegação, mostra entry anterior.
- **↓:** próxima entry, ou volta à linha original.

### 12.3 Line Editing

| Tecla | Ação |
|-------|------|
| ← | Move cursor 1 para esquerda |
| → | Move cursor 1 para direita |
| Home, ^A | Cursor para início |
| End, ^E | Cursor para fim |
| Del | Remove caractere na posição |
| Backspace | Remove caractere antes da posição |
| ^K | Kill to end (remove da posição ao fim) |
| ^U | Kill to start (remove do início à posição) |
| ^W | Kill word backward (remove palavra antes) |
| ^L | Clear screen (mantém comando atual) |

**Redesenho da linha:** após cada modificação, o shell:
1. `\r` (volta ao início da linha)
2. `\x1b[K` (limpa linha)
3. Escreve `"MkM> " + cmd`
4. `len - pos` backspaces para posicionar cursor

### 12.4 Redirecionamento

Analisado em `execute_command()` antes de despachar:

```
echo hello > file.txt    → comando="echo hello", arquivo="file.txt", append=0
echo hello >> file.txt   → comando="echo hello", arquivo="file.txt", append=1
cat < input.txt          → lê input.txt, mostra na tela (simple cat)
cmd1 | cmd2              → executa cmd1, depois cmd2 (pipe via buffer)
```

**Mecanismo de `>`:**

1. Antes de executar: `redir_active = 1`, `redir_len = 0`
2. `vga_putchar()` verifica `redir_active` e escreve em `redir_buf[]` em vez de VGA
3. Após execução: cria/trunca arquivo, chama `fat32_write_file(redir_buf)`

**Mecanismo de `>>`:**

1. Lê arquivo existente com `fat32_read_file()`
2. Concatena com buffer de saída
3. Escreve tudo de volta com `fat32_write_file()`

### 12.5 PATH Search

```c
static const char *paths[] = {"/BIN", "/USR/BIN", "/LOCAL/BIN", NULL};

// Em execute_command(), após builtins:
if (cmd_exec_in_dir(work, "/BIN") != 0 &&
    cmd_exec_in_dir(work, "/USR/BIN") != 0 &&
    cmd_exec_in_dir(work, "/LOCAL/BIN") != 0) {
    vga_puts("Comando nao encontrado: ");
}
```

`cmd_exec_in_dir()` muda para o diretório, tenta ler o arquivo, carrega Mach-O e executa.

### 12.6 Autocomplete (TAB)

O TAB handler no `shell_loop()` implementa autocomplete:

1. **First word (command):** completa contra builtins (`help`, `ls`, `cd`, etc.) e contra executáveis nos diretórios PATH (`/BIN/`, `/USR/BIN/`, `/LOCAL/BIN/`).
2. **Subsequent words:** completa contra arquivos/diretórios no CWD (Current Working Directory).
3. **Match único:** substitui o prefixo digitado pelo match completo, adiciona espaço (para comandos) ou `/` (para diretórios).
4. **Múltiplos matches:** preenche o longest common prefix automaticamente.
5. **Double-TAB:** lista todos os matches em uma linha abaixo do prompt.

```python
# Pseudocódigo do TAB handler:
if k == TAB:
    prefix = palavra_atual_no_cursor
    if primeira_palavra:
        matches = match_builtins(prefix) + match_PATH(prefix)
    else:
        matches = match_fat32(prefix, CWD)
    if len(matches) == 1:
        completar(matches[0])
    elif len(matches) > 1:
        preencher_prefixo_comum(matches)
        if tab_count >= 2:
            listar(matches)
```

A função `fat32_match_prefix()` (fat32.c) itera sobre as entradas de um diretório FAT32 e retorna entradas cujo nome começa com o prefixo dado.

### 12.7 Variáveis de Ambiente

```c
#define ENV_MAX 32
#define ENV_NAME_MAX 32
#define ENV_VAL_MAX  64
static char env_name[ENV_MAX][ENV_NAME_MAX];
static char env_val[ENV_MAX][ENV_VAL_MAX];
static int env_count = 0;
```

- `env_set(key, val)` — define ou atualiza variável.
- `env_get(key)` — retorna valor ou NULL.
- `env_init()` — define defaults: `PATH=/BIN:/USR/BIN:/LOCAL/BIN`, `HOME=/`, `EDITOR=edit`, `SHELL=MkM`, `PS1=MkM> `.
- Comando `export VAR=valor` no shell.
- `export` sem argumentos lista todas as variáveis.
- `$PATH` é usado pelo PATH search e pelo autocomplete.
- `$PS1` é lido a cada iteração do shell loop para o prompt.
- `$?` é exportado automaticamente após cada comando (0 = sucesso, 1 = erro).

### 12.8 Aliases

```c
#define ALIAS_MAX 32
static char alias_name[ALIAS_MAX][64];
static char alias_val[ALIAS_MAX][CMD_MAX];
static int alias_count = 0;
```

- `alias_set(nome, comando)` — define ou atualiza alias.
- `alias_get(nome)` — retorna expansão ou NULL.
- `alias_unset(nome)` — remove alias.
- Comandos: `alias nome='comando'`, `unalias nome`.
- `alias` sem argumentos lista todos os aliases.
- Expansão não-recursiva (expande uma vez no início de `execute_command()`).
- A expansão preserva o resto da linha (argumentos após o nome do alias).

### 12.9 Prompt Customizável (PS1)

O prompt é construído a cada iteração pela função `build_prompt()`, que lê a variável `$PS1` e expande sequências de escape:

```c
static void build_prompt(char *prompt, int maxlen) {
    const char *p = env_get("PS1");
    if (!p) p = "[\\w]\\$ ";                // default: [/]#
    int o = 0;
    for (int i = 0; p[i] && o < maxlen-1; i++) {
        if (p[i] == '\\' && p[i+1]) {
            i++;
            if (p[i] == '\\')      { prompt[o++] = '\\'; }
            else if (p[i] == 'u')  { ... "root" ... }
            else if (p[i] == 'h')  { ... "ovsb" ... }
            else if (p[i] == 's')  { ... "MkM" ... }
            else if (p[i] == '$')  { prompt[o++] = '#'; }
            else if (p[i] == 'w')  { ... fat32_get_cwd_name() ... }
            else if (p[i] == 'W')  { ... basename of CWD ... }
        } else {
            prompt[o++] = p[i];
        }
    }
    prompt[o] = '\0';
}
```

Sequências de escape suportadas:

| Escape | Expansão |
|--------|----------|
| `\u` | Nome do usuário ("root") |
| `\h` | Hostname ("ovsb") |
| `\w` | Diretório de trabalho atual (caminho completo) |
| `\W` | Nome base do diretório atual (sem caminho) |
| `\s` | Nome do shell ("MkM") |
| `\$` | `#` (root — sempre root no momento) |

A função é chamada:
- No início de `shell_loop()`
- Após cada comando (Enter), para refletir `cd`
- No `redraw:` (^L, redesenho da linha)

Isso significa que se você digitar `cd /APPS`, o prompt muda automaticamente para `[/APPS]# `.

### 12.10 Navegação de Diretório (cd / pwd)

O diretório atual é armazenado em `current_dir_cluster` (fat32.c). As funções de navegação:

- **`cd <caminho>`** — muda para o diretório especificado.
  - `cd /` — volta à raiz.
  - `cd ..` — sobe um nível (entrada `..` do FAT32 → cluster do pai).
  - `cd .` — fica no mesmo diretório (entrada `.` → próprio cluster).
  - `cd /APPS` — caminho absoluto (começa com `/`).
  - `cd APPS` — caminho relativo ao CWD.
  - `cd` (sem argumentos) — volta à raiz.
- **`pwd`** — reconstrói e exibe o caminho completo.
  - Sobe pela cadeia de entradas `..` até a raiz, coletando nomes.
  - Usa uma pilha de até 16 níveis (profundidade máxima de diretório).

A entrada `..` existe em TODO diretório FAT32 (criada por `fat32_mkdir()`). A raiz (`/`) é o único diretório que NÃO tem `..` — `fat32_change_dir("/")` trata este caso explicitamente.

> **Bug corrigido (v0.5.3):** `cd ..` falhava porque `name_to_83()` confundia o primeiro ponto de `".."` com separador de extensão, gerando `"        ."` em vez de `"..        "`. Corrigido com early return para entradas `.` e `..`.

> **Bug corrigido (v0.5.3):** `fat32_get_cwd_name()` não tratava `..` com cluster=0 (convenção FAT32 que significa "pai é a raiz"). `parent_cluster` virava 0 e a busca no pai não executava, resultando em `depth=0` → saída `/`. Corrigido com `if (parent_cluster == 0) parent_cluster = boot.root_cluster`.

### 12.11 Scripting (source)

O comando `source <arquivo>` lê até 4096 bytes de um arquivo, divide por `\n`, e executa cada linha como um comando via `execute_command()`:

```c
else if (strncmp(work, "source ", 7) == 0) {
    n = fat32_read_file(fn, sbuf, 4096);
    if (n > 0) {
        for (int i = 0; i <= n; i++) {
            if (i == n || sbuf[i] == '\n') {
                line[li] = '\0';
                if (li > 0) execute_command(line);
                li = 0;
            } else { line[li++] = sbuf[i]; }
        }
    } else {
        // "Nao encontrado: <arquivo>"
    }
}
```

### 12.12 Exit Code ($?)

Após cada comando, a variável `$?` é automaticamente exportada:

```c
static int last_exit_code = 0;

// em execute_command(), comando não encontrado
if (!found) { last_exit_code = 1; ... }

// em shell_loop(), após executar comando
char ec[16]; itoa(last_exit_code, ec, 10);
env_set("?", ec);  // $? = último exit code
```

Valores:
- `0` — comando executado com sucesso
- `1` — comando não encontrado

### 12.12 Terminal Colorido (colors.h)

O sistema de cores VGA foi extraído de `#define COLOR (0x0A)` fixo para um esquema dinâmico:

```c
// colors.h — 16 cores VGA + macros de sistema
#include "colors.h"

uint8_t vga_attr;              // atributo atual (fore|back)
void set_vga_color(uint8_t);   // muda cor do terminal

#define C_PROMPT     COLOR(WHITE, BLACK)         // prompt "MkM> "
#define C_PATH       COLOR(CYAN, BLACK)           // caminhos
#define C_COMMAND    COLOR(LIGHT_GREEN, BLACK)    // comando digitado
#define C_OUTPUT     COLOR(GRAY, BLACK)           // saída normal
#define C_ERROR      COLOR(LIGHT_RED, BLACK)      // mensagens de erro
#define C_SUCCESS    COLOR(GREEN, BLACK)          // operações bem-sucedidas
#define C_DIR        COLOR(LIGHT_CYAN, BLACK)     // diretórios (ls)
#define C_FILE       COLOR(WHITE, BLACK)          // arquivos (ls)
#define C_HEADER     COLOR(YELLOW, BLACK)         // cabeçalhos
```

Onde aplicado:
- **Prompt (shell_loop):** `C_PROMPT` para o prompt, `C_COMMAND` para o texto digitado
- **Autocomplete (double-TAB):** `C_DIR`/`C_FILE` para diretórios/arquivos
- **list_dir_at():** `C_DIR` para diretórios (com `/`), `C_FILE` para arquivos
- **Erros:** `C_ERROR` em todas as mensagens de erro (FAT32, comando não encontrado, etc.)
- **Sucesso:** `C_SUCCESS` em "Criado:", "Removido:", "OK", etc.
- **Cabeçalhos:** `C_HEADER` no boot banner, help, editor banner, exec separators
- **Boot:** banner amarelo, "FAT32 OK" verde, "Erro FAT32" vermelho

A cor padrão (`vga_attr`) é `C_OUTPUT` (gray on black), definida em startup.

## 13. Redirecionamento

O redirecionamento é implementado no kernel e funciona interceptando a saída de `vga_putchar()`.

### 13.1 Globais

```c
static int redir_active = 0;      // 0=normal, 1=coletando para arquivo
static char redir_buf[16384];     // buffer de saída
static int redir_len = 0;        // bytes no buffer
```

### 13.2 Fluxo

```
Comando: "echo hello > file.txt"

execute_command("echo hello > file.txt"):
  1. Parse: work="echo hello", redir_file="file.txt", redir_append=0
  2. redir_active = 1, redir_len = 0
  3. cmd_echo("hello") → vga_putchar('h') → redir_buf[0]='h'
                          vga_putchar('e') → redir_buf[1]='e'
                          ... → redir_buf[5]='\n' → redir_len=6
  4. redir_active = 0
  5. fat32_create_file("file.txt")
     fat32_write_file("file.txt", redir_buf, 6)
```

### 13.3 Pipe (|)

Atualmente mostra `[pipe not fully implemented yet]`. Para implementar:
- Coletar saída do primeiro comando em `redir_buf`
- Alimentar essa saída como entrada para o segundo comando
- Isso requer um "teclado virtual" que leia do buffer de pipe

---

## 14. Comandos

### 14.1 Builtins Atuais

| Comando | Função | Descrição |
|---------|--------|-----------|
| `help` | `cmd_help()` | Lista comandos |
| `clear` | `cmd_clear()` | Limpa tela |
| `echo` | `cmd_echo()` | Imprime texto |
| `about` | `cmd_about()` | Sobre o sistema |
| `shutdown` | `cmd_shutdown()` | Desliga (CLI+HLT) |
| `ls` | `cmd_ls()` | Lista diretório FAT32 |
| `touch` | `cmd_touch()` | Cria arquivo vazio |
| `rm` | `cmd_rm()` | Remove arquivo |
| `cat` | `cmd_cat()` | Mostra conteúdo |
| `edit` | `cmd_edit()` | Editor de linha único |
| `mkdir` | `cmd_mkdir()` | Cria diretório |
| `cd` | `cmd_cd()` | Muda diretório atual. Suporta `..` (pai), `.` (próprio), caminhos absolutos (`/APPS`) e relativos (`APPS`, `./APPS`). |

| `pwd` | `cmd_pwd()` | Mostra caminho completo do diretório atual (reconstruído via walking `..` chain). |
| `mv` | `cmd_mv()` | Renomeia |
| `cp` | `cmd_cp()` | Copia |
| `rmdir` | `cmd_rmdir()` | Remove diretório |
| `reboot` | `cmd_reboot()` | Reinicia (PS/2 reset + cli;hlt) |
| `source` | kernel.c | Executa comandos de um arquivo |
| `export` | kernel.c | Define/mostra variáveis de ambiente |
| `alias` | kernel.c | Define/mostra aliases |
| `unalias` | kernel.c | Remove alias |
| `stat` | `cmd_stat()` | Info arquivo (tamanho, atributos, timestamps) |
| `exec` | `cmd_exec()` | Executa Mach-O |
| `disp` | `cmd_disp()` | Modo gráfico |
| `date` | `cmd_date()` | Data/hora atual (RTC) |
| `uptime` | `cmd_uptime()` | Tempo desde boot |
| `sleep` | `sleep_ms()` | Espera N segundos |

### 14.2 Como Adicionar Comando

1. **shell_cmds.h:** declare `void cmd_meucomando(void);`
2. **shell_cmds.c:** implemente a função
3. **kernel.c** (`execute_command`): adicione o dispatch:
   ```c
   else if (strcmp(work, "meucomando") == 0) cmd_meucomando();
   ```
4. **kernel.c** (`cmd_help`): adicione à lista de help

---

## 15. Userland

### 15.1 Pipeline de Compilação

```
.c → gcc -ffreestanding -nostdlib → .elf → objcopy -O binary → .bin
  → macho_pack.py → .macho → mcopy → disk.img:/BIN/
```

### 15.2 CRT0 (crt0.c)

```c
__attribute__((naked)) void _start(void) {
    // zera BSS
    // chama main(argc, argv)
    // chama exit(ret)
    // (nunca retorna)
}
```

### 15.3 Libc

**stdio.c** — funções via int 0x80:
- `int 0x80` inline asm com XNU convention (RAX=num, RDI/RSI/RDX/RCX=args)
- `printf()` com suporte a %d, %u, %x, %s, %c
- `fopen/fread/fwrite/fclose` sobre FAT32
- `kbhit()` — verifica tecla sem bloquear

**stdlib.c** — `malloc()` bump allocator de 64KB heap estático. `free()` é no-op.

**string.c** — `strlen`, `strcmp`, `strcpy`, `strcat`, `strtok`, `memset`, `memcpy`, etc.

### 15.4 Inclusões

Userland inclui headers de:
- `include/stdio.h`, `stdlib.h`, `string.h`, `ctype.h`
- `include/sys/stat.h`

---

## 16. Graphy

### 16.1 Arquitetura

Graphy é um editor de texto TUI full-screen de ~620 linhas. Roda como Mach-O userland.

```
main():
  1. Limpa tela, esconde cursor
  2. Carrega arquivo (ou novo)
  3. Loop: screen() → rd_k() → processa tecla
  4. Sai com ^X, restaura cursor
```

### 16.2 Novas Funcionalidades

| Tecla | Ação |
|-------|------|
| ^O | Salvar |
| ^X | Sair |
| ^G | Toggle help |
| ^F | Find |
| ^S (^R) | Replace (global) |
| ^J | Go to line |
| ^Z | Undo (512 ops) |
| ^W | Cut word |
| ^Y | Paste clipboard |
| ^K | Kill line |
| ^T | Toggle buffer |
| ^C | Command mode |
| F2 | Line numbers toggle |

### 16.3 Syntax Highlighting

A função `screen()` implementa um state machine de highlighting:

```
Estados:
0 = normal
1 = dentro de string "..."
2 = dentro de line comment //...
3 = dentro de block comment /*... * 5. name_to_83(): nomes especiais "." e ".." eram corrompidos porque
 *    o primeiro ponto era tratado como separador nome.ext, gerando
 *    "        ." em vez de "..        ". Corrigido: early return para
 *    entradas especiais FAT32.
 */
4 = preprocessor #...

Cores ANSI:
- Strings: verde (\x1b[32m)
- Comments: vermelho (\x1b[31m)
- Preprocessor: ciano (\x1b[36m)
- Keywords: amarelo (\x1b[33m)
- Numbers: magenta (\x1b[35m)
- Line numbers: cinza (\x1b[37m)
```

### 16.4 Undo/Redo

```c
#define UNDO_MAX 512
static struct { int pos; char ch; int is_ins; } undo_stack[UNDO_MAX];
```

- `push_undo(pos, ch, is_ins)` — registra operação (insert=1, delete=0)
- `undo_one()` — desfaz última operação
- Buffer circular: quando cheio, sobrescreve mais antigo

### 16.5 Auto-indent

```c
// Ao pressionar Enter:
ins(co, '\n');
int ind = line_indent(cy); // conta espaços/tabs no início da linha anterior
for (int i = 0; i < ind; i++) ins(co + 1 + i, ' ');
```

---

## 17. Como Adicionar uma Syscall

### Passo 1: Defina o número

Em `syscall.c`, adicione:
```c
#define SYS_minha_syscall 999
```

### Passo 2: Implemente o handler

Em `syscall_handler()` dentro do `switch`:
```c
case SYS_minha_syscall:
    // a1, a2, a3, a4 são os argumentos
    return resultado;
```

### Passo 3: Chame do userland

```c
static inline int64_t minha_syscall(int a1, int a2) {
    int64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(999), "D"(a1), "S"(a2)
        : "memory"
    );
    return ret;
}
```

---

## 18. Como Adicionar um Comando

### Passo 1: shell_cmds.h

```c
void cmd_hello(void);
```

### Passo 2: shell_cmds.c

```c
void cmd_hello(void) {
    vga_puts("Hello, world!\n");
}
```

### Passo 3: kernel.c

```c
else if (strcmp(work, "hello") == 0) cmd_hello();
```

Adicione também ao `cmd_help`:
```c
vga_puts("hello   ...\n");
```

---

## 19. Como Adicionar uma Função na Libc

### Exemplo: Adicionar `strdup`

1. **include/string.h:** declare `char *strdup(const char *s);`
2. **string.c:** implemente:
   ```c
   char *strdup(const char *s) {
       int len = strlen(s) + 1;
       char *cpy = malloc(len);
       if (cpy) memcpy(cpy, s, len);
       return cpy;
   }
   ```

### Regras:
- Libc NÃO tem acesso a syscalls complexas (fork, mmap real — stubs existem mas são limitados)
- `malloc` é bump allocator — `free` não reusa memória
- `printf` usa buffer de 256 bytes para formatação

---

## 20. Debugging

### 20.1 QEMU + Serial

```bash
make run  # mostra VGA + serial no terminal
```

O kernel envia logs para porta serial COM1 (`0x3F8`). Para capturar:
```bash
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -serial stdio
```

### 20.2 QEMU + GDB

```bash
# Terminal 1:
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -s -S

# Terminal 2:
gdb -ex "target remote :1234" \
    -ex "symbol-file OvsbMkM/build/kernel.elf" \
    -ex "break kmain" \
    -ex "continue"
```

### 20.3 Mensagens de Debug no Kernel

```c
debug_puts("aqui chegou\n");  // escreve no VGA + serial
serial_puts("debug\n");        // só no serial
vga_puts("visivel\n");         // só no VGA
```

### 20.4 VGA como Debug

O syscall handler escreve o número da syscall nos primeiros pixels do VGA:
```c
vga[0] = (0x0E << 8) | ('0' + (num / 100 % 10));
vga[1] = (0x0E << 8) | ('0' + (num / 10 % 10));
vga[2] = (0x0E << 8) | ('0' + (num % 10));
```

---

## 21. Problemas Comuns

### "Kernel panic: no working init found"
- FAT32 não foi inicializado. Verifique `fat32_init()` > 0.
- Disk image não encontrada. `make run` monta `disk.img` automaticamente.

### "Comando nao encontrado" para executáveis no /BIN/
- Binário não está em `/BIN/`. `mcopy` falhou? Verifique `src/userland/Makefile`.
- Formato inválido. `macho_pack.py` falhou? Execute manualmente para debug.

### Tela cheia de caracteres estranhos
- ANSI escape codes sendo mostrados como texto. O parser ANSI no VGA não está ativo.
- Verifique `vga_putchar()` — precisa chamar o parser ANSI, não escrever raw.

### Teclas não respondem
- IRQ1 não habilitado? `keyboard_init()` deve ser chamado, e `sti` habilitado.
- Buffer circular cheio? `kb_head == kb_tail` indica buffer vazio ou overflow silencioso.

### Timer não funciona
- PIT não inicializado? `pit_init()` deve ser chamado em kmain().
- IRQ0 não configurado? `idt_init()` configura IRQ0 para o entry point `irq0`.
- EOI não enviado? `timer_tick_handler()` envia EOI com `outb(0x20, 0x20)`.

### build falha com `inw %edx`
- GCC 15+ gera instruções AT&T com `%edx` em vez de `%dx`. O Makefile tem regra `sed` para corrigir.

### graphy crash ao abrir arquivo grande
- Limite de 64KB (`KBUF`). Arquivos maiores truncam.
- Limite de 4096 linhas (`MAXLNS`). Mais linhas causam overflow.

### Shell não mostra history
- History é circular. `HIST_MAX=128`. Primeiro comando após boot não está no history?
- `browse_idx` tracking: verifique se `hist_count` está incrementando.

### graphy abre mas não aceita teclado
- `int $0x80` no IDT usava **interrupt gate** (type `0x0E`), que desabilita IRQs. `keyboard_read()` dentro do syscall handler nunca recebe dados.
- **Fix:** `IDT_TRAP_GATE` (`0x0F`) na entrada 0x80 (`src/kernel/idt.c:54`). Trap gate preserva IF, permitindo IRQ do teclado.

### malloc/free não reusa memória
- A libc usava bump allocator (64KB heap, `free` no-op). Programas que alocam/libram frequentemente (ex: editor com undo) vazam até o heap encher.
- **Fix:** `src/userland/libc/stdlib.c` agora usa freelist circular com coalescência. Alocações >= 2048 bytes vão direto pra `mmap` (syscall 197), aliviando o heap principal.
- Adicionados: `calloc()`, `realloc()`, `mmap()`, `munmap()`.

---

## Apêndice A: Estrutura de Diretórios

```
OvsbMkM/src/
├── kernel/
│   ├── boot64.asm         # Entry point, long mode, GDT, paging
│   ├── kernel.c           # kmain, VGA (ANSI), shell, RTC, PIT, redirect
│   ├── kernel.h           # Declarações públicas do kernel
│   ├── idt.c              # IDT setup
│   ├── idt.h              # Structs IDT
│   ├── idt.asm            # ISR stubs, IRQ handlers
│   ├── test_idt.c         # idt_handler (exceções)
│   ├── syscall.c          # 30 syscalls (XNU convention)
│   ├── syscall_entry.asm  # Entry point da syscall
│   ├── memory.c           # Bump allocator, page allocator
│   ├── memory.h           # Declarações de memória
│   ├── mach_o.c           # Carregador Mach-O 64-bit
│   ├── mach_o.h           # Structs Mach-O
│   ├── dyld.c             # Dynamic linker
│   ├── dyld.h             # Structs dyld
│   ├── dyld_bin.c         # dyld binário embutido
│   ├── bash_bin.c         # bash binário embutido
│   ├── libsystem_bin.c    # libSystem binário embutido
│   ├── ls_bin.c           # ls binário embutido
│   ├── test_macho.c       # Teste Mach-O
│   ├── pic.c              # PIC 8259 init
│   ├── smc.c              # SMC stub
│   ├── smc.h
│   ├── nvram.c            # NVRAM stub
│   ├── nvram.h
│   └── linker.ld          # Linker script
├── drivers/
│   ├── keyboard.c         # PS/2 keyboard (scancode→ASCII, repeat)
│   ├── keyboard.h
│   ├── keyboard_asm.asm   # IRQ1 handler asm
│   ├── ata.c              # ATA PIO LBA28
│   ├── ata.h
│   ├── vga_gfx.c          # VGA graphics mode 320x200
│   └── vga_gfx.h
├── commands/
│   ├── shell_cmds.c       # 21 comandos builtin
│   └── shell_cmds.h
├── fs/
│   ├── fat32.c            # FAT32 completo (748 linhas)
│   └── fat32.h

src/userland/
├── libc/
│   ├── crt0.c             # Runtime C startup
│   ├── stdio.c            # printf, fopen, fread, fwrite, etc
│   ├── stdlib.c           # malloc, free, atoi, exit
│   ├── string.c           # string.h completo
│   └── link.ld            # Linker script userland
├── include/
│   ├── stdio.h / stdlib.h / string.h / ctype.h
│   └── sys/stat.h
├── progs/
│   └── graphy.c           # Editor TUI (~620 linhas)
├── disp/
│   └── compositor.c       # Compositor gráfico
└── tools/
    └── macho_pack.py      # Empacota .bin → Mach-O
```

---

> **"Um sistema operacional não é sobre o que você pode fazer — é sobre o que você pode construir."**
>
> — TipOS Team, 2026
