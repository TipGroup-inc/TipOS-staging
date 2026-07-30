<!-- moe moe kyun <3 -->
# TipOS — Organização do Código & Equipes

Baseado no OvsbMkM (https://github.com/Bugsappetit-inc/OvsbMkM) — microkernel 64-bit com terminal VGA, driver PS/2, IDT/PIC, syscalls XNU, carregador Mach-O, SMC/NVRAM mock.

---

## 1. O que já existe (OvsbMkM)

### 1.1 Visão geral do código legado

| Arquivo | Linhas | O que faz | Aproveitamento |
|---------|--------|-----------|----------------|
| `boot64.asm` | 87 | Bootloader Multiboot2, transição 32→64-bit, PAE, PML4, GDT | **100%** — vai pra `kernel/arch/x86_64/boot.asm` |
| `linker.ld` | 31 | Linker script, posiciona seções a partir de 1 MB | **100%** — vai pra `build/kernel.ld` |
| `idt.asm` | 116 | Handlers de exceção (0-31) + IRQ0/IRQ1 + salvamento de registradores | **100%** — vai pra `kernel/arch/x86_64/idt_asm.asm` |
| `idt.c` | 60 | Inicialização da IDT, set de entries, syscall gate (int 0x80), IRQ1 | **100%** — vai pra `kernel/arch/x86_64/idt.c` |
| `idt.h` | 32 | Header com structs idt_entry_t, idt_ptr_t, defines | **100%** — vai pra `kernel/include/idt.h` |
| `pic.c` | 39 | Inicialização PIC (8259A), remapeamento IRQ0→32, IRQ1→33 | **100%** — vai pra `kernel/arch/x86_64/pic.c` |
| `kernel.c` | 104 | kmain(): init + shell + VGA driver (buffer 0xB8000, scroll, cursor) | **80%** — VGA vira driver separado, shell vira app |
| `kernel.h` | 12 | Header básico do kernel | **100%** — vai pra `kernel/include/kernel.h` |
| `memory.c` | 83 | Bump allocator (kmalloc/kfree), bitmap de páginas, mmap_user/munmap_user | **70%** — será substituído por allocador real |
| `memory.h` | 22 | Header com defines PROT_*, MAP_* | **70%** — expandir |
| `syscall.c` | 91 | Syscalls estilo XNU: write, read, open, close, mmap, getpid, etc. | **50%** — expandir de 15 para 150+ chamadas |
| `syscall_entry.asm` | 39 | Entry point para int 0x80, passa args rax/rdi/rsi/rdx para C | **100%** — vai pra `kernel/arch/x86_64/syscall_entry.asm` |
| `keyboard.c` | 56 | Driver PS/2: buffer circular, scancode→ASCII, init, IRQ handler | **100%** — vai pra `drivers/input/ps2_keyboard.c` |
| `keyboard.h` | 11 | Header do teclado | **100%** |
| `keyboard_asm.asm` | 37 | Wrapper assembly para IRQ1 do teclado | **100%** |
| `mach_o.c` | 49 | Carregador Mach-O básico: parse header, LC_SEGMENT_64, LC_UNIXTHREAD | **70%** — precisa de relocação, símbolos, binding |
| `mach_o.h` | 74 | Headers: mach_header_64_t, segment_command_64_t, section_64_t, LC_* | **90%** — adicionar mais LC_* |
| `smc.c` | 18 | Mock SMC (Apple System Management Controller) | **30%** — placeholder, expandir depois |
| `smc.h` | 9 | Header SMC | **30%** |
| `nvram.c` | 21 | Mock NVRAM (boot-args) | **30%** — placeholder |
| `nvram.h` | 10 | Header NVRAM | **30%** |
| `test_idt.c` | 18 | idt_handler(): mostra K (teclado), T (timer), E (exceção) no VGA | **50%** — lógica de debug separada |
| `test_macho.c` | 15 | Stub Mach-O para testar o loader | **30%** — substituir por binário real |
| `mini_shell.c` | 39 | Mini shell user-space que usa int 0x80 para syscalls | **50%** — base para o shell nativo |

### 1.2 O que NÃO mudar (alterações do usuário preservadas)

O usuário ainda não fez alterações — o código é o original do OvsbMkM. Qualquer modificação futura deve ser feita nos arquivos já mapeados acima, dentro da nova estrutura.

---

## 2. Estrutura TipOS (diretórios)

```
tipos/
├── kernel/                     # Microkernel (Ring 0)
│   ├── arch/
│   │   └── x86_64/
│   │       ├── boot.asm        ← boot64.asm (idêntico)
│   │       ├── idt.c           ← idt.c
│   │       ├── idt_asm.asm     ← idt.asm
│   │       ├── pic.c           ← pic.c
│   │       ├── syscall_entry.asm ← syscall_entry.asm
│   │       └── gdt.c           (novo) — GDT dinâmica para user-space
│   ├── include/
│   │   ├── kernel.h            ← kernel.h
│   │   ├── idt.h               ← idt.h
│   │   ├── memory.h            ← memory.h (expandido)
│   │   ├── mach_o.h            ← mach_o.h
│   │   ├── syscall.h           (novo) — tabela de syscalls
│   │   └── types.h             (novo) — tipos básicos
│   ├── init/
│   │   └── kmain.c             ← kernel.c (sem VGA e shell)
│   ├── mm/
│   │   └── pmm.c               ← memory.c (expandido: físico + virtual)
│   ├── ipc/
│   │   └── mach_ipc.c          (novo) — portas, mensagens, memória compartilhada
│   ├── sched/
│   │   ├── scheduler.c         (novo) — escalonador preemptivo
│   │   └── pcb.h               (novo) — estrutura de processo
│   └── syscall/
│       └── syscall.c           ← syscall.c (expandido)
│
├── drivers/                    # User-space (Ring 3)
│   ├── input/
│   │   ├── ps2_keyboard.c      ← keyboard.c
│   │   └── ps2_keyboard.h      ← keyboard.h
│   └── video/
│       └── vga.c               ← VGA extraído do kernel.c
│
├── servers/                    # Servidores de sistema
│   ├── wm/                     (futuro) — WindowServer
│   ├── vfs/                    (futuro) — Virtual File System
│   ├── netsrv/                 (futuro) — lwIP
│   └── audiosrv/               (futuro) — Áudio
│
├── libs/
│   ├── libc/                   (novo) — libc mínima (str*, printf, malloc)
│   ├── libtipos/                (novo) — syscall wrappers nativos
│   └── libmach/                (novo) — IPC Mach para apps
│
├── apps/
│   ├── shell/
│   │   └── shell.c             ← mini_shell.c (expandido)
│   └── init/
│       └── init.c              (novo) — primeiro processo, carrega shell
│
├── translators/                (futuro)
│   ├── linux/                  — tradutor de syscalls Linux
│   ├── wine/                   — Windows (sobre Linux)
│   └── macho/                  — macOS (nativo)
│
├── build/
│   ├── Makefile                ← Makefile (adaptado)
│   ├── kernel.ld               ← linker.ld
│   └── grub.cfg                ← do iso/boot/grub/
│
├── docs/
│   └── architecture.md         ← ARCHITECTURE_PHASE1.md + README.md
│
└── iso/                        — estrutura da ISO
```

---

## 3. Equipes de Desenvolvimento

### Time A — Núcleo do Kernel (Kernel Core)

**Responsabilidades:** boot, memória, escalonador, processos, syscalls.

| Membro | Tarefa | Arquivos |
|--------|--------|----------|
| **Dev A1** | Boot + transição 64-bit | `kernel/arch/x86_64/boot.asm`, `build/kernel.ld` |
| **Dev A2** | IDT + PIC + GDT + APIC | `kernel/arch/x86_64/idt.c`, `idt_asm.asm`, `pic.c`, `gdt.c` |
| **Dev A3** | Alocador de memória física | `kernel/mm/pmm.c` (bitmap → free list) |
| **Dev A4** | Memória virtual + paging | `kernel/mm/vmm.c` (identidade → demanda) |
| **Dev A5** | Escalonador + PCB | `kernel/sched/scheduler.c`, `pcb.h` |
| **Dev A6** | Syscalls (expandir para 150+) | `kernel/syscall/syscall.c` |

**Milestone:** Kernel roda 2 processos em user-space com memória isolada.

---

### Time B — IPC e Drivers (IPC & Drivers)

**Responsabilidades:** comunicação entre processos, drivers de dispositivos.

| Membro | Tarefa | Arquivos |
|--------|--------|----------|
| **Dev B1** | IPC Mach — portas e mensagens | `kernel/ipc/mach_ipc.c` |
| **Dev B2** | Driver PS/2 (teclado) | `drivers/input/ps2_keyboard.c` |
| **Dev B3** | Driver VGA (modo texto) | `drivers/video/vga.c` |
| **Dev B4** | ACPI + PCI discovery | `drivers/bus/pci.c` (novo) |
| **Dev B5** | Timer HPET/APIC timer | `kernel/arch/x86_64/timer.c` (novo) |
| **Dev B6** | Driver AHCI (SATA) | `drivers/storage/ahci.c` (novo — Fase 3) |

**Milestone:** Servidor de janelas mínimo + teclado via IPC.

---

### Time C — Libs e Compatibilidade (Libraries & Compatibility)

**Responsabilidades:** libc, carregadores de binários, tradutores.

| Membro | Tarefa | Arquivos |
|--------|--------|----------|
| **Dev C1** | libc mínima (printf, sprintf, strlen, memcpy) | `libs/libc/` |
| **Dev C2** | Carregador Mach-O (completar) | `kernel/ipc/ ou kernel/syscall/ — mach_o.c` |
| **Dev C3** | Carregador ELF | `kernel/syscall/elf_loader.c` (novo) |
| **Dev C4** | Syscalls XNU (expandir) | `kernel/syscall/syscall.c` (junto com A6) |
| **Dev C5** | Shell nativo | `apps/shell/shell.c` ← mini_shell.c |
| **Dev C6** | Tradutor Linux (futuro) | `translators/linux/` |

**Milestone:** Executar binário ELF estático (busybox) via shell.

---

### Time D — Aplicações e User-Space (Apps & User Experience)

**Responsabilidades:** apps nativas, servidores de sistema, interface.

| Membro | Tarefa | Arquivos |
|--------|--------|----------|
| **Dev D1** | Init (primeiro processo) | `apps/init/init.c` |
| **Dev D2** | WindowServer básico | `servers/wm/window_server.c` |
| **Dev D3** | Paint (app de desenho) | `apps/paint/` |
| **Dev D4** | Edit (editor de texto) | `apps/edit/` |
| **Dev D5** | Servidor VFS + FAT32 | `servers/vfs/` |
| **Dev D6** | Servidor de áudio | `servers/audiosrv/` |

**Milestone:** Interface gráfica com 2 apps rodando.

---

## 4. Mapa de Dependências

```
boot.asm ──► kmain.c ──► idt_init() ──► pic_init()
                │              │
                │              └── idt_set_syscall()
                │              └── idt_set_irq1()
                │
                ├── memory_init()
                ├── keyboard_init()
                ├── smc_init()
                └── nvram_init()
                     │
                     ▼
              Shell loop (main)
                │
                ├── help / clear / echo / about / shutdown
                ├── test → kmalloc() + mach_o_load()
                └── (futuro) → spawn() → user-space
```

### Dependências entre times:

```
Time A (kernel core) ── fornece syscalls ──► Time C (libc/shell)
       │                                            │
       └── fornece IPC ──► Time B (drivers) ────────┘
                                                      │
                            Time D (apps) ◄────────────┘
```

---

## 5. Roadmap com Times (8 fases)

### Fase 1 — Fundações (semanas 1-4) → Times A + B + C

| Tarefa | Time | Depends on |
|--------|------|-----------|
| Boot + IDT + PIC | A1, A2 | — |
| Bump allocator → free list bitmap | A3 | A1 |
| Syscalls básicas (exit, yield, spawn, wait, sleep) | A6 | A2 |
| IPC Mach (portas, mensagens) | B1 | A6 |
| Driver PS/2 via IRQ + IPC | B2 | B1, A2 |
| VGA como driver separado | B3 | — |
| Carregador Mach-O completo | C2 | A6 |
| libc mínima | C1 | — |
| Shell rodando como user-space | C5 | C1, A6 |

### Fase 2 — Processos e Memória (semanas 5-8) → Time A

| Tarefa | Time |
|--------|------|
| Alocador de páginas com bitmap | A3 |
| Paginação demanda (page fault handler) | A4 |
| PCB + escalonador round-robin | A5 |
| Syscall spawn → criar processo user-space | A6 |
| Transição Ring 3 → Ring 0 (syscall/sysret) | A2 |

### Fase 3 — Armazenamento (semanas 9-12) → Time B

| Tarefa | Time |
|--------|------|
| PCI enumeration | B4 |
| Driver AHCI | B6 |
| Servidor VFS | D5 |
| FAT32 driver | D5 |
| Syscalls de arquivo (open, read, write, close) | A6 |

### Fase 4 — Rede (semanas 13-16) → Time B

| Tarefa | Time |
|--------|------|
| Driver Intel e1000 | B (novo) |
| Port lwIP | B |
| Syscalls socket | A6 |

### Fase 5 — Compatibilidade Linux (semanas 17-20) → Time C

| Tarefa | Time |
|--------|------|
| Carregador ELF | C3 |
| Mapeamento 150 syscalls Linux → nativas | C4 + A6 |
| Shell Linux (busybox) rodando | C5 |

### Fase 6 — Modos de Performance (semanas 21-24) → Time A

| Tarefa | Time |
|--------|------|
| Syscall map_physical | A4 |
| IOPB por processo | A6 |
| /ring0, /ring3, /perf, /realtime | A6 + Shell |

### Fase 7 — Vídeo e Áudio (semanas 25-32) → Time B + D

| Tarefa | Time |
|--------|------|
| Driver i915 framebuffer | B |
| Port do Mesa (software→Vulkan) | B |
| WindowServer | D2 |
| Paint + Edit | D3, D4 |
| Driver HDA + servidor áudio | D6 |

### Fase 8 — Virtualização (semanas 33-40) → Time A + C

| Tarefa | Time |
|--------|------|
| Hypervisor VT-x/AMD-V | A |
| Wine sobre tradutor Linux | C |
| Tradutor Mach-O nativo | C |

---

## 6. Regras de Contribuição

1. **Cada time tem um branch** — `team-a/`, `team-b/`, etc. Merge para `main` a cada milestone.
2. **Código novo em C11** — sem extensões de compilador além das necessárias.
3. **Assembly mínimo** — só onde C não alcança (boot, IDT, troca de contexto).
4. **Headers em `kernel/include/`** — um header por subsistema.
5. **Drivers em user-space** — desde o início. O kernel só tem o necessário para IPC e gestão de processos.
6. **Testar no QEMU antes de commitar** — `make run` tem que bootar sem crash.
7. **Log via porta serial** — `outb(0x3F8, c)` para debug, não poluir VGA.

---

## 7. Makefile (adaptado para TipOS)

```makefile
CC := x86_64-elf-gcc
AS := nasm
LD := x86_64-elf-ld
GRUB := grub-mkrescue
QEMU := qemu-system-x86_64

BUILD := build
ISO := tipos.iso
CFLAGS := -ffreestanding -nostdlib -mno-red-zone -mgeneral-regs-only -Wall -O0 -I kernel/include
LDFLAGS := -T build/kernel.ld

KERNEL_OBJS := \
    $(BUILD)/boot.o \
    $(BUILD)/idt_asm.o \
    $(BUILD)/syscall_entry.o \
    $(BUILD)/kmain.o \
    $(BUILD)/idt.o \
    $(BUILD)/pic.o \
    $(BUILD)/pmm.o \
    $(BUILD)/scheduler.o \
    $(BUILD)/syscall.o \
    $(BUILD)/mach_ipc.o

DRIVER_OBJS := \
    $(BUILD)/ps2_keyboard.o \
    $(BUILD)/keyboard_asm.o \
    $(BUILD)/vga.o

all: $(BUILD)/tipos.elf

run: $(ISO)
    $(QEMU) -cdrom $< -m 512M -serial stdio

debug: $(ISO)
    $(QEMU) -cdrom $< -m 512M -s -S -serial stdio &
    $(GDB) -ex "target remote :1234" -ex "symbol-file $(BUILD)/tipos.elf"

$(ISO): $(BUILD)/tipos.elf
    mkdir -p iso/boot/grub
    cp $< iso/boot/
    cp build/grub.cfg iso/boot/grub/
    $(GRUB) -o $@ iso

$(BUILD)/tipos.elf: $(KERNEL_OBJS) $(DRIVER_OBJS)
    $(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: kernel/arch/x86_64/%.asm
    $(AS) -f elf64 -o $@ $<

$(BUILD)/%.o: kernel/arch/x86_64/%.c
    $(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: kernel/*/
    $(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: drivers/*/
    $(CC) $(CFLAGS) -c -o $@ $<
```

---

## 8. Checklist de Migração Imediata

- [ ] Clonar OvsbMkM para `tipos/`
- [ ] Renomear arquivos conforme estrutura acima
- [ ] Adaptar Makefile para nova estrutura
- [ ] Separar VGA de kmain.c para `drivers/video/vga.c`
- [ ] Separar shell de kmain.c para `apps/shell/shell.c`
- [ ] Testar `make run` — deve mostrar "TipOS" no lugar de "OvsbMkM"
- [ ] Criar branches `team-a/`, `team-b/`, `team-c/`, `team-d/`
- [ ] Publicar no GitHub: `github.com/levementesalgado/tipos`
