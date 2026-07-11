# Arquitetura Técnica - Fase 1 do OvsbMkM (v2.0 - Atualizado)

## 1. Fluxo de Boot Completo (ATUAL)

### 1.1 GRUB → Kernel (Multiboot2)

```
QEMU inicia com flag: -cdrom OvsbMkM.iso
        ↓
BIOS/UEFI carrega GRUB da ISO
        ↓
GRUB lê grub.cfg e encontra kernel.elf
        ↓
GRUB carrega kernel.elf na memória (1MB = 0x100000)
        ↓
GRUB busca magic number Multiboot2 (0xE85250D6) nos primeiros 32KB
        ↓
GRUB valida checksum e tags Multiboot2
        ↓
GRUB seta CPU em modo protegido 32-bit
        ↓
GRUB salta para _start do kernel
        ↓
EAX = magic number (0x36D76289)
EBX = pointer para estrutura multiboot2_info
```

**Por que GRUB em vez de `-kernel` direto?**
- `qemu -kernel` exige PVH ELF Note (problemático)
- GRUB é o bootloader padrão para PCs reais
- ISO bootável funciona no QEMU e em hardware físico
- Mesmo processo de desenvolvimento e deploy final

### 1.2 Bootloader (boot64.asm) — Transição 32→64-bit

**Arquivo:** `boot64.asm`
**Formato:** ELF64 com cabeçalho Multiboot2

O bootloader realiza as seguintes etapas:

#### Etapa 1: Multiboot2 Header
```asm
section .multiboot
align 8
multiboot_header:
    dd 0xE85250D6          ; magic Multiboot2
    dd 0                   ; arch (i386)
    dd header_end - multiboot_header
    dd -(0xE85250D6 + 0 + (header_end - multiboot_header))
    dw 0, 0, 8             ; end tag
header_end:
```

#### Etapa 2: Habilitar PAE (Physical Address Extension)
```asm
mov eax, cr4
or eax, 1 << 5            ; Bit 5 (PAE)
mov cr4, eax
```

#### Etapa 3: Carregar PML4 (Page Map Level 4)
```asm
mov eax, pml4_table       ; Endereço físico da PML4
mov cr3, eax              ; CR3 aponta para PML4
```

#### Etapa 4: Habilitar Long Mode (EFER.LME)
```asm
mov ecx, 0xC0000080       ; MSR EFER
rdmsr
or eax, 1 << 8            ; Bit 8 (LME)
wrmsr
```

#### Etapa 5: Habilitar Paginação + Modo Protegido
```asm
mov eax, cr0
or eax, 0x80000001        ; PG (bit 31) + PE (bit 0)
mov cr0, eax
```

#### Etapa 6: Carregar GDT 64-bit
```asm
lgdt [gdt64_ptr]          ; Carrega GDT com descritores 64-bit
```

#### Etapa 7: Far Jump para 64-bit
```asm
jmp 0x08:start64          ; Seletor 0x08 = code 64-bit
```

#### Etapa 8: Inicialização 64-bit
```asm
bits 64
start64:
    mov ax, 0x10           ; Seletor de dados
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    call kmain             ; Chamar kernel C
```

### 1.3 Estrutura da GDT 64-bit

```
GDT[0] = Null descriptor
GDT[1] = Code 64-bit (seletor 0x08)
         - 0x0020980000000000
         - L=1 (Long Mode), P=1, DPL=0, Type=0xA
GDT[2] = Data 64-bit (seletor 0x10)
         - 0x0000920000000000
         - P=1, DPL=0, Type=0x2 (writable)
```

### 1.4 Tabelas de Paginação

```
PML4 (Page Map Level 4) — 512 entradas de 64 bits
  └─ PDP (Page Directory Pointer) — 512 entradas
      └─ PD (Page Directory) — 512 entradas (2MB pages)
```

Mapeia os primeiros **1 GB** de RAM (512 × 2MB) como identity mapping.

**Seção dedicada no linker:**
```ld
.paging : {
    *(.paging)
}
```
Garante que as tabelas estejam alinhadas a 4096 bytes.

### 1.5 Kernel C (kmain) — 64-bit

**Arquivo:** `kernel.c`
**Compilador:** GCC nativo (Linux) com flags:
```bash
gcc -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only -Wall -O0
```

- `-ffreestanding`: Sem bibliotecas padrão
- `-nostdlib`: Sem link com libc
- `-mno-red-zone`: Desabilita red zone (incompatível com kernel)
- `-mgeneral-regs-only`: Usa apenas registradores gerais (sem SIMD)
- `-O0`: Sem otimização (debug mais fácil)

## 2. Drivers: VGA (Modo Texto)

### 2.1 Framebuffer VGA

Buffer em `0xB8000` (memória física mapeada pela paginação).

```
0xB8000: Caractere (col,lin) | Atributo (cor)
```

Total: 80 × 25 × 2 bytes = 4000 bytes

### 2.2 Cores

```c
#define COLOR (0x0A)  // Fundo preto (0) + Texto verde claro (10)
```

Cada entrada VGA:
```c
vga[pos] = (COLOR << 8) | caractere;
```

### 2.3 Controle do Cursor

```c
outb(0x3D4, 0x0F);           // Registrador cursor low
outb(0x3D5, pos & 0xFF);
outb(0x3D4, 0x0E);           // Registrador cursor high
outb(0x3D5, (pos >> 8) & 0xFF);
```

### 2.4 Scroll

Quando o cursor ultrapassa a linha 24, todo o texto sobe uma linha:
```c
for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
    vga[i] = vga[i + VGA_WIDTH];
for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
    vga[i] = (COLOR << 8) | ' ';
```

## 3. Driver PS/2 (Teclado)

### 3.1 Portas I/O

```c
#define PS2_DATA   0x60   // Dados (scancode)
#define PS2_STATUS 0x64   // Status (bit 0 = dados disponíveis)
```

### 3.2 Leitura (Polling)

```c
uint8_t ps2_read() {
    while (!(inb(PS2_STATUS) & 0x01));  // Espera bit 0 = 1
    return inb(PS2_DATA);               // Lê scancode
}
```

### 3.3 Tabela de Scancodes (ASCII)

```c
static const char sc_ascii[] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,
    0,'q','w','e','r','t','y','u','i','o','p','[',']',0,
    0,'a','s','d','f','g','h','j','k','l',';',0,0,0,
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    0,' ',...
};
```

### 3.4 Teclas Especiais

| Scancode | Significado |
|----------|-------------|
| 0x1C | Enter |
| 0x0E | Backspace |
| 0x2A | Left Shift (pressionado) |
| 0xAA | Left Shift (solto) |

**Nota:** Shift ainda não implementado (Fase 2).

## 4. Parser de Comandos

### 4.1 Buffer de Entrada

```c
char cmd[256];
int len = 0;
```

Loop de leitura:
```c
while (1) {
    uint8_t sc = ps2_read();
    if (sc == 0x1C) {        // Enter
        cmd[len] = 0;
        break;
    } else if (sc == 0x0E) { // Backspace
        if (len > 0) len--;
    } else if (sc < 128) {
        char c = sc_ascii[sc];
        if (c && len < 255) cmd[len++] = c;
    }
}
```

### 4.2 Comandos Implementados

| Comando | Função |
|---------|--------|
| `help` | Lista comandos disponíveis |
| `clear` | Limpa a tela |
| `echo <texto>` | Repete o texto |
| `about` | Informações do sistema |
| `shutdown` | Para a CPU (`cli; hlt`) |

### 4.3 Comparação de Strings

```c
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
```

## 5. Mapa de Memória

```
0x00000000 ┌──────────────────────┐
           │ IVT + BDA (BIOS)     │
0x00007C00 ├──────────────────────┤
           │ GRUB (stage 1)       │
0x00010000 ├──────────────────────┤
           │ GRUB (stage 2)       │
0x00100000 ├──────────────────────┤ ← Kernel carregado aqui (1 MB)
           │ .multiboot (header)  │
           │ .text (código)       │
           │ .rodata (strings)    │
           │ .data (variáveis)    │
           │ .paging (tabelas)    │
           │ .bss (não inicial.)  │
           │ Stack (16 KB)        │
0x00104000 ├──────────────────────┤
           │ Livre                │
0xFFFFFFFF └──────────────────────┘
```

## 6. Fluxo do Terminal Interativo

```
kmain()
  ├─ vga_clear()                    // Limpa tela
  ├─ vga_puts("MkM Terminal...")   // Mensagem inicial
  └─ while(1) {
       ├─ vga_puts("MkM> ")        // Prompt
       ├─ while(1) {               // Loop de leitura
       │    sc = ps2_read()         //   Lê scancode
       │    if (sc == Enter) break  //   Enter → executar
       │    if (sc == Backspace)    //   Backspace → apagar
       │    else                    //   Caractere → eco + buffer
       │  }
       ├─ if (len == 0) continue   // Linha vazia
       ├─ strcmp/strncmp           // Comparar comandos
       └─ executar ação            // help/clear/echo/about/shutdown
     }
```

## 7. Porquê Estas Escolhas?

### Por que GRUB + Multiboot2?
- Funciona no QEMU **e** em hardware real (pendrive bootável)
- Não depende de PVH ELF Note (problemático no QEMU)
- GRUB já configura modo protegido 32-bit
- Padrão da indústria para kernels

### Por que Transição Manual para 64-bit?
- GRUB Multiboot2 não coloca automaticamente em Long Mode
- Dá controle total sobre GDT e paginação
- Essencial para entender o hardware

### Por que Seção `.paging` Separada?
- Alinhamento 4096 obrigatório para tabelas
- Linker script garante posicionamento correto
- Evita conflitos com outras seções

### Por que `-mgeneral-regs-only`?
- Evita que o GCC use registradores XMM/SSE
- Esses registradores exigem salvamento de estado
- Não estamos prontos para isso na Fase 1

### Por que Polling (não Interrupções)?
- Mais simples de implementar
- Funciona perfeitamente para terminal
- IDT será adicionada na Fase 2

## 8. Build e Execução

### 8.1 Compilação

```bash
# Bootloader
nasm -f elf64 -o build/boot64.o boot64.asm

# Kernel C
gcc -ffreestanding -nostdlib -mno-red-zone -mno-mmx -mno-sse \
    -mgeneral-regs-only -Wall -O0 -c -o build/kernel.o kernel.c

# Linkagem
ld -T linker.ld -o build/kernel.elf build/boot64.o build/kernel.o
```

### 8.2 Criação da ISO

```bash
cp build/kernel.elf iso/boot/
grub-mkrescue -o OvsbMkM.iso iso/
```

### 8.3 Execução

```bash
qemu-system-x86_64 -cdrom OvsbMkM.iso -m 256M
```

### 8.4 Estrutura de Arquivos (Final)

```
~/OvsbMkM/
├── boot64.asm          # Bootloader Multiboot2 + transição 64-bit
├── kernel.c            # Terminal interativo 64-bit
├── linker.ld           # Linker script
├── iso/
│   └── boot/
│       └── grub/
│           └── grub.cfg
├── build/
│   ├── boot64.o
│   ├── kernel.o
│   └── kernel.elf
└── OvsbMkM.iso         # ISO bootável final
```

## 9. Limitações Atuais

1. **Sem IDT** — Exceções causam Triple Fault
2. **Sem shift** — Apenas letras minúsculas
3. **Sem sistema de arquivos** — Apenas shell em RAM
4. **Sem multitarefa** — Único fluxo de execução
5. **Sem proteção de memória** — Tudo em Ring 0

## 10. Próximos Passos (Fase 2)

- Adicionar IDT para tratamento de exceções
- Implementar shift para maiúsculas/símbolos
- Cores no terminal (prompt verde, comandos coloridos)
- Histórico de comandos (buffer circular)
- Migrar para `x86_64-elf-gcc` (quando disponível)

---

**Última atualização:** 2026-06-21 — Terminal 64-bit funcional! ✅