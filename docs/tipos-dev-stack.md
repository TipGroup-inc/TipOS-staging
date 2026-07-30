<!-- moe moe kyun <3 -->
# TipOS — Fluxo de Desenvolvimento & Stack

---

## 1. Nome

**TipOS** (substituindo Kora OS).  
Motivo: nome curto, sem conflito com outros projetos.

---

## 2. Stack Tecnológica

### Linguagens

| Camada | Linguagem | Motivo |
|--------|-----------|--------|
| **Kernel** (microkernel) | **C11** (GCC) | Padrão da indústria para kernels. Controle total sobre memória, sem runtime. Cross-compiler maduro. |
| **Bootloader / assembly** | **NASM** (x86-64) | Obrigatório para entry point, IDT, APIC, transição pra long mode. |
| **Drivers & Servidores** (user-space) | **C** (inicial) / **Rust** (futuro) | C pra começar (compartilha libc com kernel). Rust depois pra drivers de GPU/rede onde safety compensa. |
| **Scripts de build** | **Make** + Bash | Universal, sem dependências pesadas. |

### Toolchain (cross-compiler)

```
Target: x86_64-elf
- binutils 2.42+
- gcc 14+ (apenas c, sem libc)
- nasm 2.16+
- ld (do binutils)
- grub-mkrescue (para ISO)
```

Build do cross-compiler (uma vez):

```bash
export PREFIX="$HOME/.local/cross"
export TARGET=x86_64-elf

# Binutils
../binutils-2.42/configure --target=$TARGET --prefix=$PREFIX --disable-nls
make -j$(nproc) && make install

# GCC (apenas C, sem libc)
../gcc-14.2.0/configure --target=$TARGET --prefix=$PREFIX \
  --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc && make install-gcc install-target-libgcc
```

### Teste & Debug

| Ferramenta | Uso |
|-----------|-----|
| **QEMU** (>= 8.0) | Emulação rápida, suporte a KVM, debug via GDB stub |
| **GDB** (multiarch) | Debug remoto do kernel via `target remote :1234` |
| **OVMF** (UEFI) | Testar boot em modo UEFI |
| **Log serial** (`-serial stdio`) | Principal canal de logging do kernel |

### Sistema hospedeiro

Slackware 15 (meu sistema) ou qualquer Linux. Dependências mínimas:

```
gcc make nasm qemu xorriso mtools python3
```

---

## 3. Estrutura de Diretórios

```
tipos/
├── boot/               # Bootloader (own or Limine config)
│   └── multiboot2/
├── kernel/             # Microkernel
│   ├── arch/
│   │   └── x86_64/     # CPU-specific: idt, gdt, apic, paging, syscall
│   ├── ipc/            # Mach-style ports, messages, shared memory
│   ├── mm/             # Physical + virtual memory manager
│   ├── sched/          # Scheduler, PCB, threads
│   ├── syscall/        # Syscall table and handlers
│   ├── cap/            # Capability system (IOPB, MMIO regions)
│   └── init/           # Kernel entry, main
├── servers/            # User-space system servers
│   ├── wm/             # Window server
│   ├── vfs/            # Virtual file system server
│   ├── netsrv/         # Network server (lwIP)
│   └── audiosrv/       # Audio server
├── drivers/            # User-space drivers
│   ├── gpu/            # i915 framebuffer
│   ├── input/          # PS/2 keyboard, mouse, USB HID
│   ├── storage/        # AHCI, NVMe
│   ├── net/            # e1000, RTL8139
│   └── audio/          # HDA
├── libs/               # Libraries
│   ├── libc/           # Minimal POSIX-like C library
│   ├── libtipos/        # TipOS-native syscall wrappers
│   ├── libmach/        # Mach IPC client library
│   └── libui/          # Windowing toolkit
├── translators/        # Syscall translation layers
│   ├── linux/          # Linux ELF binary support
│   ├── wine/           # Windows binary support
│   └── macho/          # macOS binary support
├── apps/               # Native applications
│   ├── shell/          # Interactive shell
│   ├── paint/          # Drawing app
│   └── edit/           # Text editor
├── docs/               # Documentation
├── build/              # Build scripts, linker scripts
└── iso/                # ISO directory structure (bootable)
```

---

## 4. Ciclo de Desenvolvimento

### 4.1 Loop principal

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Editar  │────▶│  Compilar│────▶│  Rodar   │────▶│  Debug   │
│  código  │     │  (make)  │     │ (qemu)   │     │ (gdb/log)│
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                       ▲                                │
                       └────────────────────────────────┘
                              (corrigir erros)
```

### 4.2 Comandos

```makefile
# Makefile (raiz)
all: tipos.iso

run: tipos.iso
    qemu-system-x86_64 -cdrom $< -m 512M -serial stdio

debug: tipos.iso
    qemu-system-x86_64 -cdrom $< -m 512M -s -S -serial stdio &
    gdb -ex "target remote :1234" -ex "symbol-file kernel/tipos.elf"

tipos.iso: kernel/tipos.elf
    grub-mkrescue -o $@ iso/

kernel/tipos.elf: $(wildcard kernel/**/*.c kernel/**/*.asm)
    $(MAKE) -C kernel
```

### 4.3 Workflow prático

1. **Edita** um arquivo `.c` ou `.asm` no kernel
2. **`make run`** → compila, gera ISO, abre QEMU
3. Vê logs no terminal serial
4. Se crashou: **`make debug`** → QEMU pausa, GDB conecta
5. Analisa backtrace, corrige, repete

---

## 5. Fase 1 — Próximos Passos Concretos

Baseado no protótipo existente (boot 64-bit, VGA, teclado PS/2, IDT/PIC), o que fazer **agora**:

### 5.1 Infraestrutura

- [ ] Criar o `Makefile` cross-compilado (x86_64-elf-gcc)
- [ ] Configurar linker script (`kernel.ld`) com seções `.text`, `.rodata`, `.data`, `.bss`
- [ ] Migrar o código existente para a estrutura `tipos/kernel/arch/x86_64/`
- [ ] Adicionar log serial via porta COM1 (`outb(0x3F8, c)`) — mais confiável que VGA para debug

### 5.2 Paginação

- [ ] Ativar long mode com identidade mapeada para primeiros 2 MiB
- [ ] Implementar alocador físico (bitmap ou free list)
- [ ] `kmalloc`/`kfree` sobre páginas de 4 KiB

### 5.3 Processos

- [ ] Estrutura PCB (registradores, estado, pilha do kernel)
- [ ] Escalonador round-robin simples
- [ ] Troca de contexto (switch entre processos)
- [ ] Syscalls: `exit()`, `yield()`, `spawn()`, `sleep()`, `wait()`

### 5.4 Carregadores

- [ ] Carregador **Mach-O** (já prototipado no doc) — completar
- [ ] Carregador **ELF** básico (cabeçalhos, segmentos, entry point)

### 5.5 IPC básico

- [ ] Criar um canal de comunicação kernel↔processo via porta
- [ ] Enviar IRQ do teclado para um processo servidor

---

## 6. Decisões Arquiteturais

| Decisão | Opção escolhida | Alternativa |
|---------|----------------|-------------|
| **Bootloader** | Manter o próprio (OvsbMkM) por enquanto. Se precisar de UEFI/GOP, migrar para **Limine** | GRUB legado (mais complexo para UEFI) |
| **IPC** | Mach-style portas (conforme doc) — mensagens assíncronas com memória partilhada | seL4-style sync IPC (mais rápido, mas API diferente) |
| **Syscall gate** | `syscall`/`sysret` (x86-64) — mais rápido que `int 0x80` | `int 0x80` (legado) |
| **Scheduler** | Round-robin com prioridades (começar simples) | Lottery scheduling, O(1) |
| **Formato de binário nativo** | Mach-O (já prototipado). ELF como secundário para compatibilidade | ELF-only (mais simples, mas perderia o protótipo existente) |
| **Sistema de arquivos inicial** | FAT32 (leitura/escrita, fácil de implementar) | ext2 (mais complexo) |
| **Rede** | lwIP portado como servidor user-space | Implementação própria (muito trabalho) |
| **Áudio** | Port do PulseAudio ou servidor simples inicialmente | ALSA (muito acoplado ao Linux) |

---

## 7. Referências e Recursos

| O quê | Link |
|-------|------|
| OSDev Wiki (bíblia do dev de SO) | https://wiki.osdev.org |
| Limine bootloader (moderno, UEFI + BIOS) | https://github.com/limine-bootloader/limine |
| lwIP (stack TCP/IP leve) | https://savannah.nongnu.org/projects/lwip/ |
| Redox OS (referência de SO em Rust) | https://doc.redox-os.org |
| ToaruOS (microkernel educacional em C) | https://github.com/klange/toaruos |
| seL4 (microkernel verificado) | https://sel4.systems |
| Mach IPC spec (original CMU) | https://www.cs.cit.tum.de/fileadmin/w00cfhd/papers/1991-mach-ipc.pdf |
| Intel SDM (manual da CPU) | https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-2a-manual.html |
| QEMU + GDB debug | https://wiki.osdev.org/Kernel_Debugging |

---

## 8. Checklist Pra Iniciar

```bash
# 1. Criar toolchain cross-compiler
cd ~/src
wget https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz
wget https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz
# ... seguir receita da seção 2

# 2. Estruturar diretórios
mkdir -p ~/tipos/{kernel/arch/x86_64,kernel/ipc,kernel/mm,kernel/sched,kernel/syscall,servers,drivers,libs,boot,iso/boot/grub}

# 3. Criar Makefile e linker script
# 4. Mover código do protótipo existente
# 5. Testar: make run
```

---

*Documento escrito em 2026-07-09. Auxilia o "Kora OS Doc v1" como referência principal.*
