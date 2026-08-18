<!-- moe moe kyun <3 -->
# TipOS Kernel — Documentação Completa

> **Nível:** Bizarro. Até uma pedra consegue continuar o desenvolvimento.
>
> **Arquitetura:** x86-64, long mode, ring 0, GRUB/Multiboot2, single-address-space.
> **Linguagens:** C11 (GCC) + NASM (assembly) + Zig (`zig build-obj`, `x86_64-freestanding`).
> **Build:** Makefile + GRUB + QEMU.

---

## Índice

1. [Arquitetura Geral](#1-arquitetura-geral)
2. [Boot Step-by-Step](#2-boot-step-by-step)
3. [Layout de Memória](#3-layout-de-memória)
4. [IDT — Interrupt Descriptor Table](#4-idt)
5. [Syscalls](#5-syscalls)
6. [VGA Text Mode + ANSI Parser](#6-vga)
7. [VESA Framebuffer (VBE)](#7-vesa-framebuffer-vbe)
8. [Teclado PS/2](#8-teclado)
9. [ATA PIO](#9-ata)
10. [FAT32](#10-fat32)
11. [PIT — Timer Programável](#11-pit)
12. [RTC — Relógio de Tempo Real](#12-rtc)
13. [Shell](#13-shell)
14. [Redirecionamento e Pipes](#14-redirecionamento)
15. [Comandos](#15-comandos)
16. [Userland e Libc](#16-userland)
17. [Graphy — Editor TUI](#17-graphy)
18. [Como Adicionar uma Syscall](#18-como-adicionar-uma-syscall)
19. [Como Adicionar um Comando](#19-como-adicionar-um-comando)
20. [Como Adicionar uma Função Libc](#20-como-adicionar-uma-função-libc)
21. [Debugging](#21-debugging)
22. [Problemas Comuns](#22-problemas-comuns)
23. [ELF64 Loader (musl static PIE)](#23-elf64-loader-musl-static-pie)
24. [Linux x86-64 Compatibility](#24-linux-x86-64-compatibility)

---

## 1. Arquitetura Geral

```
                    ┌─────────────────────────────────────────┐
                    │             USERLAND (ring 3)           │
                    │   graphy (TUI)  |  disp-wm (compositor) │
                    │   ELF64 (musl)  |  Mach-O (libc própria)│
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
2. **boot64.asm** (`OvsbMk/kernel/boot64.asm`):
   - Recebe controle em 32-bit protected mode.
   - Configura **PML4 → PDP → PD** com páginas de 2MB (identity mapping dos primeiros 1GB).
   - Habilita **PAE**, **long mode** (EFER.LME = 1), **paging** (CR0.PG = 1).
   - Carrega **GDT** (64-bit CS/DS).
   - Salta via `far jmp` para código 64-bit.
   - Inicializa stack (0x90000), zera BSS, chama `kmain()`.

### 2.2 kmain() — Sequência de Inicialização

```c
void kmain(uint32_t magic, uint32_t mb_info) {
    idt_init();              // 1. Configura IDT (exceções, IRQs, handlers)
    pic_init();              // 2. PIC 8259 — remapeia IRQs 0-15 para vetores 32-47
    idt_set_syscall();       // 3. Registra int 0x80 como syscall gate (DPL=3)
    idt_set_irq1();          // 4. Registra handler customizado para IRQ1 (teclado)
    idt_set_irq12();         // 5. IRQ12 para mouse PS/2
    keyboard_init();         // 6. Habilita IRQ1 no PIC
    pit_init();              // 7. Programa PIT para 100 Hz
    memory_init();           // 8. Inicializa heap (bump allocator 64MB @ 0x900000)
    proc_init();             // 9. Tabela de processos + idle task
    tss_init();              // 10. TSS para ring 3
    __asm__ ("sti");         // 11. Habilita interrupções
    parse_multiboot2(        // 12. Lê tags Multiboot2 (framebuffer VESA)
        magic, mb_info);
    mouse_init();            // 13. Inicializa mouse PS/2 (se fb ativo)
    ata_init();              // 14. ATA PIO init (primary master)
    fat32_init();            // 15. Lê BPB, monta FAT32
    disp_init();             // 16. Compositor gráfico (se fb ativo)
    vga_puts("TipOS...\n");  // 17. Mensagens de boot
    shell_loop();            // 18. → SHELL (nunca retorna)
}
```

**Ordem importa:** PIC antes de habilitar IRQs, VESA antes do boot selector (framebuffer), ATA antes de FAT32.

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
| 200 | `disp_get_fb` | Mapeia FB no userland + dimensões | syscall.c:397 |
| 201 | `disp_flush` | Copia backbuffer → FB (rep movsl) | syscall.c:437 |
| 202 | `mouse_read` | Lê delta dx/dy/botões do PS/2 | syscall.c:424 |
| 203 | `kb_mod` | Estado shift/ctrl (bit0=shift, bit1=ctrl) | syscall.c:492 |
| 204 | `lseek` | Posiciona em arquivo | syscall.c:219 |
| 205 | `disp_flush_rect` | Flush de retângulo (x,y em a2, w,h em a3, 16 bits cada) | syscall.c:461 |
| 207 | `readdir` | Lista diretório FAT32 | syscall.c:506 |
| 208 | `execve` | Executa ELF | syscall.c:525 |
| 209 | `shell_cmd` | Comando do shell | syscall.c:593 |
| 210 | `spawn` | Cria processo | syscall.c:614 |
| 211 | `spawn_shared` | Cria processo com memória compartilhada | syscall.c:704 |

### 5.3 Syscalls display/input (200-205) — para compositores

Estáveis (ABI congelada para o disp-wm):

| # | Nome | Parâmetros | Retorno |
|---|------|-----------|---------|
| 200 | `disp_get_fb` | a1=&addr (u64\*), a2=&width (u32\*), a3=&height (u32\*), a4=&pitch (u32\*) | 0 ou -1 se FB inativo |
| 201 | `disp_flush` | a1=backbuffer | 0 (copia pitch×height via rep movsl, sem SSE) |
| 202 | `mouse_read` | a1=&dx (i32\*), a2=&dy (i32\*), a3=&buttons (i32\*) | 0 (zera acumulador) |
| 203 | `kb_mod` | — | bitmask: bit0=shift, bit1=ctrl |
| 205 | `disp_flush_rect` | a1=backbuffer, a2=x\|\|y<<16, a3=w\|\|h<<16 | 0 |

Notas:
- **FB VA**: mapeado em `0xFFFFFFFF80000000` (2MB pages, U/S=1, RW) — mesmo endereço que o kernel usa pra VESA. Formato: 32-bit, pitch em bytes (`pitch/4` = stride em pixels).
- **Ordem dos parâmetros** do `disp_get_fb`: a2=width, a4=pitch. Histórico: já esteve trocado (a2=pitch, a4=width) — não reverter!
- **`disp_flush`** usa `rep movsl` no kernel (ERMSB/QEMU otimizam); sem SSE (kernel não salva xmm).
- **`mouse_read`** consome o acumulado do driver PS/2 (IRQ12, `kernel/drivers/mouse.zig`) — deltas são relativos à última leitura.
- **`kb_mod`** lê as flags `shift_pressed`/`ctrl_pressed` do teclado.

### 5.3.1 Misc syscalls Linux (issue #52) — para libc/weston

Stubs "realistas": devolvem valores corretos mas sem trabalho pesado. Destravam binários complexos.

| Linux # | Nome | Comportamento |
|---------|------|---------------|
| 63 | `uname` | sysname=Linux, release=TipOS, machine=x86_64 |
| 318 | `getrandom` | preenche com xorshift (não cripto) |
| 229 | `clock_getres` | resolução 10ms (PIT 100Hz) |
| 98 | `getrusage` | struct zerada |
| 100 | `times` | tms com timer_ticks global |
| 99 | `sysinfo` | uptime + totalram=64MB (heap do kernel) |
| 110 | `getppid` | `parent_pid` do PCB |
| 121 | `getpgid` | cada processo é grupo de si |
| 95 | `umask` | guarda e devolve a anterior (default 0777) |
| 72 | `fcntl` | F_GETFD/F_SETFD/F_GETFL/F_SETFL (O_NONBLOCK no stdin!) |
| 32 | `dup` | duplica fd (menor livre) |
| 91 | `dup2` | duplica em fd específico (fecha antes) |
| 292 | `dup3` | dup2 + flags |
| 22 | `pipe` | par de fds com buffer de 4KB compartilhado |
| 293 | `pipe2` | pipe + O_NONBLOCK/O_CLOEXEC |

Notas:
- **O_NONBLOCK**: `fds[0].flags & O_NONBLOCK` faz o `read` de stdin retornar `-EAGAIN` (`-11`) quando não há tecla — weston configura o stdin em nonblock no boot.
- **Colisões resolvidas no `syscall_linux.zig`**: `dup2`(33)→91, `recvmsg`(47)→92, `fcntl`(54→na real é 72, identity)→, `rt_sigprocmask`(134)→94, `tkill`(200)→95, `futex`(202)→96, `sched_setaffinity`(203)→99, `sched_getaffinity`(204)→101, `tgkill`(234)→103 — pois os números originais colidem com syscalls TipOS. **Importante**: `dup` no Linux x86_64 é **32**, não 23 (23 é `select`).
- **pipe**: 8 buffers globais (`MAX_PIPES`), `pipe_idx` liga os 2 fds; `close_fd` libera o buffer quando um fd do par fecha.

### 5.4 Fluxo de Syscall

```
Userland: RAX=num, RDI=a1, RSI=a2, RDX=a3 → int 0x80
  ↓
syscall_entry.asm: salva regs, rearranja args para C calling convention
  ↓
syscall_handler(num, a1, a2, a3, a4) → switch(num) → executa handler
  ↓
syscall_entry.asm: restaura regs → iretq → retorna para userland
```

### 5.5 gettimeofday — Implementação Real

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

## 7. VESA Framebuffer (VBE)

### 7.1 Visão Geral

O kernel suporta VESA BIOS Extensions (VBE) via **Multiboot2 framebuffer tag (type 5)**.
GRUB alterna o modo gráfico antes de passar controle ao kernel, e a tag no header
multiboot2 informa endereço, pitch, largura, altura e BPP.

**Funções do driver:**

- `vesa_init()` — mapeia framebuffer físico na paginação (via `pml4_map_phys`)
- `vesa_draw_pixel(x, y, color)` — escreve 1 pixel 32-bit com bounds check
- `vesa_draw_rect(x, y, w, h, color)` — preenche retângulo (com bounds check)
- `vesa_draw_char(x, y, c, color)` — desenha glyph 8x8 (dobre altura = 16px)
- `vesa_draw_cell(x, y, c, fg, bg)` — bg + glyph em buffer local, depois memcpy atômico
- `vesa_draw_text(x, y, text, color)` — string simples
- `vesa_fill_screen(color)` — preenche tela toda (unrolled 8x)

### 7.2 Framebuffer Terminal

Quando o framebuffer está ativo (`g_fb_active == 1`), o terminal nativo
`vga_putchar()` **ignora o buffer VGA text 0xB8000** e usa:

```
fb_buf[fb_rows][fb_cols]   → buffer de caracteres (RAM)
fb_render_cell(col, row)    → vesa_draw_cell() → temp buffer + memcpy
fb_scroll()                 → shift character buffer + redraw all
fb_reset()                  → clear + set dimensions from framebuffer
```

### 7.3 Inicialização (kmain)

```c
parse_multiboot2(magic, mb_info);   // lê tag type 8 (framebuffer)
if (g_fb.addr && g_fb.bpp == 32) {
    vesa_init(&g_fb);               // mapeia FB na paginação
    g_fb_active = 1;
    fb_reset();                     // ajusta cols/rows, limpa tela
}
```

Dimensões calculadas: `fb_cols = width / 8`, `fb_rows = height / 16`.

### 7.4 Terminal nativo framebuffer vs VGA text

| Característica | VGA text (0xB8000) | VESA framebuffer |
|---|---|---|
| Buffer | 80×25 words, fixo | fb_buf[][] dinâmico (até 160×64) |
| Scroll | memcpy words | shift buffer + fb_redraw_all() |
| Cursor | hardware (0x3D4/0x3D5) | software (via fb_buf) |
| Cores | 16 VGA indexadas | 32-bit RGB mapeadas via paleta |
| Fonte | BIOS (9×16) | bitmap 8×8 (doblada para 8×16) |

### 7.5 Controle de Flicker (Renderização Atômica)

O framebuffer é memória uncacheable (PCI). Escrever pixel a pixel causa tearing
visível. Solução:

1. **`vesa_draw_cell()`** — preenche um buffer `cell[16][8]` na stack (RAM cacheada),
   depois copia os 512 bytes para o FB com 8 writes desenrolados por linha.
   Nenhum pixel aparece parcialmente (célula atômica).

2. **`fb_scroll()`** — em vez de copiar pixels no FB (que causava "estática descendo"),
   só mexe no character buffer (RAM) e redesenha todas as células atomicamente.

### 7.6 Compositor (`disp`)

O comando `disp` ativa o modo gráfico com janelas sobrepostas:

- **VESA ativo** (`g_fb_active == 1`): fundo azul escuro (`0x00224466`),
  janelas com título em `vesa_draw_char()`, cursor branco WASD,
  resolução real do framebuffer (ex: 1024×768)
- **Fallback VGA**: modo texto 80×25 (ANSI parser) via `console.c`, sem framebuffer

```c
void disp_init(void) {
    if (g_fb_active) {
        scr_w = g_fb.width;   // 1024
        scr_h = g_fb.height;  // 768
        gfx_mode = 1;
    } else {
        scr_w = 320; scr_h = 200;
        gfx_mode = 0;
    }
}
```

### 7.7 Arquivos

| Arquivo | Função |
|---------|--------|
| `OvsbMk/lib/gui/vesa.c` | Driver VESA + font 8×8 |
| `OvsbMk/lib/gui/vesa.h` | Struct framebuffer_t, declarações |
| `OvsbMk/lib/gui/gui.c` | GUI helpers (futuro) |
| `OvsbMk/kernel/boot64.asm` | Header Multiboot2 com tag framebuffer type 5 |
| `src/userland/disp-wm` | Compositor ring 3 (repo irmão, `exec DISP`) |

---

## 8. Teclado PS/2

### 8.1 Hardware

- Controlador PS/2, porta de dados `0x60`, porta de status `0x64`.
- IRQ1 (vetor 33) dispara quando uma tecla é pressionada/solta.

### 8.2 Keyboard Handler (keyboard_asm.asm + keyboard.zig)

```
IRQ1 → keyboard_irq_handler (asm) → keyboard_handler() (C)
  → lê scancode de 0x60
  → process_scancode() → traduz para ASCII ou sequência VT100
  → coloca no buffer circular kb_buffer[256]
  → EOI (0x20 para 0x20)
  → iretq
```

### 8.3 Scancode → ASCII (Set 1)

Array `norm[]` (sem shift) e `shf[]` (com shift):
- Scancode é usado como índice no array.
- `0x2A` e `0x36` = shift pressionado, `0xAA` e `0xB6` = shift solto.
- Teclas estendidas (prefixo `0xE0`): setas, Home/End, etc. → emitem sequências VT100 (`\x1b[A`, etc.).

### 8.4 Keyboard Repeat System

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

**Para ajustar:** mude `REPEAT_DELAY` (ticks) e `REPEAT_RATE` (tick interval) em `keyboard.zig`.

### 8.5 keyboard_read() — Bloqueante (buffer-only)

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

## 9. ATA PIO

### 9.1 Driver PIO (ata.c)

- **Portas:** 0x1F0–0x1F7 (primary IDE, master).
- **LBA28:** setor de 512 bytes, endereçamento linear de 28 bits.
- **Operações:**
  - `ata_read_sector(uint32_t lba, uint8_t *buf)` — espera DRQ, lê 256 words.
  - `ata_write_sector(uint32_t lba, uint8_t *buf)` — espera DRQ, escreve 256 words.
- **Init:** `ata_init()` — espera 1 segundo (busy loop), seleciona drive 0.

### 9.2 Curiosidade GCC 15+

O compilador GCC 15+ gera `inw %edx` em vez de `inw %dx`. O Makefile aplica um `sed` para corrigir o assembly gerado por GCC:

```makefile
OvsbMk/build/drivers/ata.o: OvsbMk/drivers/ata.c
    $(CC) $(CFLAGS) -S $< -o $*.s
    sed -i 's/inw %edx/inw %dx/g; s/outw %edx/outw %dx/g' $*.s
    as --64 $*.s -o $@
```

---

## 10. FAT32

### 10.1 Inicialização (fat32_init)

```c
int fat32_init(void) {
    // Lê setor 0 (boot sector / VBR)
    // Extrai: bytes_per_sector, sectors_per_cluster, reserved_sectors,
    //         fat_count, fat_size (sectors), root_cluster
    // Guarda em globais
    // Retorna 0 em caso de sucesso
}
```

### 10.2 API Completa

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

### 10.3 Arquitetura Interna

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

### 10.4 Nomes de Arquivo

- Conversão para 8.3 (uppercase, remove extensão longa):
  - `name_to_83("hello.c", out)` → `"HELLO   C  "` (8+3, padding com espaços)
  - `name_to_83("Makefile", out)` → `"MAKEFILE   "` (sem extensão)
  - `name_to_83("file.txt.bak", out)` → `"FILE~1  BAK"` (8.3 truncado)

### 10.5 Estrutura de Diretório no Disco

```
TipOS usa disk.img de 64MB FAT32.
Partições:
  /BIN/       → binários Mach-O (GRAPHY, etc)
  /USR/BIN/   → PATH secundário (futuro)
  /LOCAL/BIN/ → PATH terciário
```

---

## 11. PIT

### 11.1 Inicialização (pit_init)

```c
static void pit_init(void) {
    uint32_t div = 1193182 / 100; // 100 Hz (frequência base do PIT = 1.193182 MHz)
    outb(0x43, 0x36);             // Canal 0, lobyte/hibyte, modo 3 (square wave)
    outb(0x40, div & 0xFF);       // Low byte do divisor
    outb(0x40, (div >> 8) & 0xFF); // High byte
}
```

### 11.2 Timer Tick Handler

```c
volatile uint64_t timer_ticks = 0;  // incrementado a cada IRQ0 (~100Hz)

void timer_tick_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20); // EOI — avisa PIC que IRQ0 foi tratada
}
```

Chamado pelo entry point `irq0` em `idt.asm`, que salva registradores, chama `timer_tick_handler()`, restaura e executa `iretq`.

### 11.3 Funções Utilitárias

```c
static void sleep_ms(uint64_t ms) {
    uint64_t target = timer_ticks + ms / 10 + 1; // +1 para arredondar
    while (timer_ticks < target) { __asm__ volatile ("pause"); }
}
```

**Atenção:** `sleep_ms` é busy-wait. Não há scheduler, então a CPU fica 100% ocupada durante o sleep.

---

## 12. RTC

### 12.1 CMOS Ports

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

### 12.2 Leitura (rtc_read)

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

### 12.3 Comandos `date` e `uptime`

`date` → lê RTC, formata `YYYY-MM-DD HH:MM:SS`.
`uptime` → lê `timer_ticks / 100`, formata `Xd HH:MM`.

---

## 13. Shell

### 13.1 Arquitetura

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

### 13.2 Shell History

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

### 13.3 Line Editing

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

### 13.4 Redirecionamento

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

### 13.5 PATH Search

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

### 13.6 Autocomplete (TAB)

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

### 13.7 Variáveis de Ambiente

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

### 13.8 Aliases

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

### 13.9 Prompt Customizável (PS1)

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

### 13.10 Navegação de Diretório (cd / pwd)

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

### 13.11 Scripting (source)

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

### 13.12 Exit Code ($?)

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

### 13.12 Terminal Colorido (colors.h)

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

## 14. Redirecionamento

O redirecionamento é implementado no kernel e funciona interceptando a saída de `vga_putchar()`.

### 14.1 Globais

```c
static int redir_active = 0;      // 0=normal, 1=coletando para arquivo
static char redir_buf[16384];     // buffer de saída
static int redir_len = 0;        // bytes no buffer
```

### 14.2 Fluxo

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

### 14.3 Pipe (|)

Atualmente mostra `[pipe not fully implemented yet]`. Para implementar:
- Coletar saída do primeiro comando em `redir_buf`
- Alimentar essa saída como entrada para o segundo comando
- Isso requer um "teclado virtual" que leia do buffer de pipe

---

## 15. Comandos

### 15.1 Builtins Atuais

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

### 15.2 Como Adicionar Comando

1. **`OvsbMk/kernel/shell.c`:** declare a função estática (ex: `static void cmd_meucomando(const char *args);`)
2. **`shell.c` (`execute`):** adicione o dispatch na cadeia de `else if`:
   ```c
   else if (strieq(cmd, "meucomando", 10) && cmd_len == 10) cmd_meucomando(args);
   ```
3. **`shell.c` (`cmd_help`):** adicione à lista de ajuda

---

## 16. Userland

### 16.1 Pipeline de Compilação

```
.c → gcc -ffreestanding -nostdlib → .elf → objcopy -O binary → .bin
  → macho_pack.py → .macho → mcopy → disk.img:/BIN/
```

### 16.2 CRT0 (crt0.c)

```c
__attribute__((naked)) void _start(void) {
    // zera BSS
    // chama main(argc, argv)
    // chama exit(ret)
    // (nunca retorna)
}
```

### 16.3 Libc

**stdio.c** — funções via int 0x80:
- `int 0x80` inline asm com XNU convention (RAX=num, RDI/RSI/RDX/RCX=args)
- `printf()` com suporte a %d, %u, %x, %s, %c
- `fopen/fread/fwrite/fclose` sobre FAT32
- `kbhit()` — verifica tecla sem bloquear

**stdlib.c** — `malloc()` bump allocator de 64KB heap estático. `free()` é no-op.

**string.c** — `strlen`, `strcmp`, `strcpy`, `strcat`, `strtok`, `memset`, `memcpy`, etc.

### 16.4 Inclusões

Userland inclui headers de:
- `include/stdio.h`, `stdlib.h`, `string.h`, `ctype.h`
- `include/sys/stat.h`

---

## 17. Graphy

### 17.1 Arquitetura

Graphy é um editor de texto TUI full-screen de ~620 linhas. Roda como Mach-O userland.

```
main():
  1. Limpa tela, esconde cursor
  2. Carrega arquivo (ou novo)
  3. Loop: screen() → rd_k() → processa tecla
  4. Sai com ^X, restaura cursor
```

### 17.2 Novas Funcionalidades

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

### 17.3 Syntax Highlighting

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

### 17.4 Undo/Redo

```c
#define UNDO_MAX 512
static struct { int pos; char ch; int is_ins; } undo_stack[UNDO_MAX];
```

- `push_undo(pos, ch, is_ins)` — registra operação (insert=1, delete=0)
- `undo_one()` — desfaz última operação
- Buffer circular: quando cheio, sobrescreve mais antigo

### 17.5 Auto-indent

```c
// Ao pressionar Enter:
ins(co, '\n');
int ind = line_indent(cy); // conta espaços/tabs no início da linha anterior
for (int i = 0; i < ind; i++) ins(co + 1 + i, ' ');
```

---


## 18. Como Adicionar uma Syscall

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

## 19. Como Adicionar um Comando

Todos os comandos vivem em `OvsbMk/kernel/shell.c` (comandos de sistema ficam no kernel; userland roda via `exec`).

### Passo 1: Implemente em shell.c

```c
static void cmd_hello(const char *args) {
    console_write("Hello, world!\n");
}
```

### Passo 2: Dispatch em execute()

```c
else if (strieq(cmd, "hello", 5) && cmd_len == 5) cmd_hello(args);
```

### Passo 3: cmd_help

```c
console_write("  hello                Diz oi pra você, baka\n");
```

---

## 20. Como Adicionar uma Função na Libc

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

## 21. Debugging

### 21.1 QEMU + Serial

```bash
make run  # mostra VGA + serial no terminal
```

O kernel envia logs para porta serial COM1 (`0x3F8`). Para capturar:
```bash
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -serial stdio
```

### 21.2 QEMU + GDB

```bash
# Terminal 1:
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw -s -S

# Terminal 2:
gdb -ex "target remote :1234" \
    -ex "symbol-file OvsbMk/build/kernel.elf" \
    -ex "break kmain" \
    -ex "continue"
```

### 21.3 Mensagens de Debug no Kernel

```c
debug_puts("aqui chegou\n");  // escreve no VGA + serial
serial_puts("debug\n");        // só no serial
vga_puts("visivel\n");         // só no VGA
```

### 21.4 VGA como Debug

O syscall handler escreve o número da syscall nos primeiros pixels do VGA:
```c
vga[0] = (0x0E << 8) | ('0' + (num / 100 % 10));
vga[1] = (0x0E << 8) | ('0' + (num / 10 % 10));
vga[2] = (0x0E << 8) | ('0' + (num % 10));
```

---

## 22. Problemas Comuns

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

## 23. ELF64 Loader (musl static PIE)

### 23.1 Visão Geral

O `elf64.zig` carrega binários **ELF64** para execução nativa no TipOS,
suportando especificamente **musl-linked static PIE** (Position Independent
Executable). O loader cria um **child PML4** por processo e mapeia os segmentos
PT_LOAD com páginas de **2MB (hugepages)**.

### 23.2 Fluxo de Carga

```
elf64_load(name, binary, size) → proc_entry * (ou NULL)
  1. Verifica magic ELF (0x7F 'ELF')
  2. Valida e_machine=EM_X86_64, e_type=ET_DYN (PIE)
  3. Itera program headers (e_phoff, e_phnum)
  4. Para cada PT_LOAD:
     a. Alinha VA base com 2MB (hugepage)
     b. Aloca páginas físicas no page allocator
     c. Mapeia no child PML4 (clone da identidade, U/S gerenciado)
     d. Copia dados do segmento do binário
  5. Salta para entry point (e_entry)
```

### 23.3 mapped[] Bugfix

```zig
// ANTES (bug): bitwise OR entre VA e PA
mapped[slot] = va | phys;  // corrompe phys se VA e PA têm bits 23:21 sobrepostos

// DEPOIS (fix):
mapped[slot] = (va >> 32) << 32 | phys;
```

O `mapped[]` array armazena 32 mapeamentos de VA→PA. O bug ocorria porque
VA (ex: `0x1000000`) e PA (ex: `0x1XX000`) compartilhavam bits na faixa 23:21,
e o OR bitwise corrompia o endereço físico armazenado, causando crashes ao
acessar a memória do ELF carregado.

### 23.4 Arquivo

| Arquivo | Função |
|---------|--------|
| `OvsbMk/kernel/elf64.zig` | ELF64 loader (PML4 child, PT_LOAD, 2MB hugepages) |

---

## 24. Linux x86-64 Compatibility

### 24.1 Visão Geral

Para executar binários Linux não-modificados, o TipOS implementa uma **camada
de tradução de syscalls** que mapeia números de syscall Linux para os números
nativos do TipOS, mais stubs para syscalls Linux não presentes no TipOS nativo.

### 24.2 Syscall Translation (`syscall_linux.zig`)

| Linux # | Nome | → TipOS # | Notas |
|---------|------|-----------|-------|
| 0 | read | 3 | Mapeado diretamente |
| 1 | write | 4 | Mapeado diretamente |
| 12 | brk | 12 | Stub (retorna sucesso) |
| 60 | exit | 1 | Linux exit → TipOS exit |
| 186 | set_tid_address | 186 | Stub (retorna 0) |
| 218 | set_tid_address (alt) | 186 | Stub |
| 228 | clock_gettime | 116 | RTC-based |
| 231 | exit_group | 212 | Mapeado para 212 para evitar colisão |
| 35 | nanosleep | - | Stub (busy-wait) |
| 158 | arch_prctl | 158 | Configura FS.base via MSR |

### 24.3 Auxiliary Vector (`setup_linux_user_stack()`)

Em `process.c`, `setup_linux_user_stack()` empurra o **auxiliary vector**
no topo da stack do usuário, seguindo o layout Linux x86-64:

```
[stack top]
  AT_RANDOM (16 bytes random)
  AT_PAGESZ (4096)
  AT_SECURE (0)
  AT_PHNUM  (nº de program headers)
  AT_PHENT  (sizeof(Elf64_Phdr))
  AT_PHDR   (endereço base dos program headers)
  NULL      (terminador)
  environ   (NULL)
  argv[1]   (NULL)
  argv[0]   (nome do binário)
  argc      (1)
```

### 24.4 TLS via FS.base

O `switch.asm` salva/restaura o **MSR_FS_BASE** durante a troca de contexto,
permitindo que cada processo tenha seu próprio **Thread Local Storage** (TLS).
A syscall `arch_prctl` (Linux #158) permite que o binário musl configure o
FS.base apontando para o descritor de thread (TLS).

### 24.5 U/S Bit Management

- **`clone_identity_tables()`** — clona as PML4/PDP/PD do kernel mas **strips
  o bit U/S** (User/Supervisor) de todas as entradas, garantindo que ring 3
  não acesse páginas do kernel.
- **Spawn paths** — ao carregar um ELF, as páginas de código e stack recebem
  explicitamente o bit U/S (0x07 em vez de 0x03 para PDE/PML4E).

### 24.6 Demo: `HELLO`

```
[/]# exec HELLO
Hello from musl ELF!
[/]#
```

O comando `shell_init` executa `HELLO` primeiro, depois `DISP`, demonstrando
a execução ELF no boot. O binário HELLO é um musl-linked static PIE que
escreve no stdout e sai com código 0.

### 24.7 Arquivos

| Arquivo | Função |
|---------|--------|
| `OvsbMk/kernel/elf64.zig` | ELF64 loader (PML4 child, 2MB hugepages) |
| `OvsbMk/kernel/syscall_linux.zig` | Tradução Linux→TipOS |
| `OvsbMk/kernel/switch.asm` | FS.base (MSR_FS_BASE) save/restore |
| `OvsbMk/kernel/process.c` | `setup_linux_user_stack()` aux vector |
| `OvsbMk/kernel/memory.c` | `clone_identity_tables()`, U/S management |

---

## Apêndice A: Estrutura de Diretórios

```
/
├── OvsbMk/                  ← KERNEL (ring 0)
│   ├── kernel/
│   │   ├── boot64.asm         # Entry point, long mode, GDT, paging
│   │   ├── kernel.c           # kmain, init de tudo (VGA, IDT, FS, shell)
│   │   ├── console.c          # VGA texto 80x25 + parser ANSI
│   │   ├── shell.c            # Shell (execute, history, autocomplete, PATH)
│   │   ├── idt.c / idt.asm    # IDT setup + ISR/IRQ stubs
│   │   ├── syscall.c          # 30 syscalls (XNU convention)
│   │   ├── syscall_entry.asm  # Entry point da syscall
│   │   ├── syscall_linux.zig  # Tradução Linux→TipOS (compat)
│   │   ├── memory.c           # Bump + buddy (4KB frames) + SLAB
│   │   ├── vm_map.c           # Mapeamentos virtuais (mmap_user)
│   │   ├── process.c          # PCB (64 slots), spawn/exit/waitpid, aux vector
│   │   ├── switch.asm         # Context switch + FS.base (MSR_FS_BASE)
│   │   ├── tss.c              # Ring 3 (TSS, iretq)
│   │   ├── mach_o.c           # Carregador Mach-O 64-bit
│   │   ├── elf64.zig          # ELF64 loader (musl static PIE, child PML4)
│   │   ├── pic.c / pit.c / rtc.c / serial.c / env.c / utils.c
│   │   ├── owt_app.c          # Demo do OWT (widget toolkit)
│   │   └── linker.ld          # Linker script
│   ├── kernel/drivers/
│   │   ├── keyboard.zig       # PS/2 keyboard (scancode→ASCII, repeat)
│   │   └── mouse.zig          # PS/2 mouse
│   ├── drivers/
│   │   ├── ata.c              # ATA PIO LBA28
│   │   ├── pci.c              # PCI enumeration
│   │   ├── usb.c              # USB (stub)
│   │   ├── virtio_gpu.c       # Virtio GPU
│   │   └── keyboard_asm.asm   # IRQ1 handler asm
│   ├── fs/
│   │   ├── fat32.c            # FAT32 completo (read/write/create/delete)
│   │   ├── ext2.zig           # ext2 parcial
│   │   └── initramfs.zig      # Initramfs
│   ├── lib/
│   │   ├── gui/               # vesa.c (framebuffer 1024x768x32)
│   │   ├── owt/               # Widget toolkit (button, label, textbox...)
│   │   └── wm/                # Window manager (multi-janela, backbuffer)
│   ├── iso/                   # grub.cfg + kernel.elf para ISO
│   ├── tests/                 # HELLO/TTEST (musl static PIE, demo ELF)
│   └── Makefile               # Build do kernel (C + ASM + Zig)

src/userland/                 ← Userland (ring 3) — + repos irmãos ../disp, ../term
├── libc/                     # crt0, stdio, stdlib, string, ctype
├── include/                  # stdio.h, stdlib.h, string.h, ctype.h, sys/stat.h
├── progs/
│   └── graphy.c              # Editor TUI (~620 linhas, syntax highlight)
└── tools/
    └── macho_pack.py         # Empacota .bin → Mach-O
```

---

> **"Um sistema operacional não é sobre o que você pode fazer — é sobre o que você pode construir."**
>
> — TipOS Team, 2026
