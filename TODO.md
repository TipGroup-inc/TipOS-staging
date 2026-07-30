<!-- moe moe kyun <3 -->
# TipOS — TODO Supremo

> **Visão:** Sistema operacional TUI-first focado em desenvolvimento de software
> e uso corporativo. Experiência tipo IDE integrada (editor + terminal + compilador + git),
> sem bloat de GUI, mas com usabilidade de ferramentas modernas.
>
> **Público-alvo:** Programadores sérios, engenheiros de software, devops,
> profissionais que passam o dia no terminal.

---

## ⚙️ Legenda

| Marca | Significado |
|-------|-------------|
| ✅ | Pronto / funcional |
| 🔸 | Parcial / stub |
| ❌ | Não existe |
| 📌 | Prioridade máxima |
| 💡 | Diferencial competitivo do TipOS |

---

## 📦 1. O QUE JÁ TEM (Inventário Completo)

### 1.1 Kernel Core

| Componente | Status | Arquivo |
|------------|--------|---------|
| Boot 64-bit + Multiboot2 + long mode | ✅ | `boot64.asm` |
| GDT + paginação (PML4 → PDP → PD, 2MB pages, identity 1GB) | ✅ | `boot64.asm` |
| IDT (32 ISRs exceptions, IRQ0 timer, IRQ1 keyboard) | ✅ | `idt.c` |
| Syscall gate int 0x80 (XNU convention, 30 handlers) | ✅ | `syscall.c`, `syscall_entry.asm` |
| PIC 8259 (IRQs remapeados 32-47) | ✅ | `pic.c` |
| Serial COM1 debug output | ✅ | `kernel.c` |
| ATA PIO (LBA28, read/write sector, BSY polling fix) | ✅ | `ata.c` |
| FAT32 (init, read, write, create, delete, mkdir, rmdir, rename, stat, chdir, pwd) | ✅ | `fat32.c` |
| Bump allocator kmalloc (64MB heap @ 0x900000-0x4900000) | ✅ | `memory.c` |
| Page allocator (bitmap, 4096-byte pages, 1024 pages) | ✅ | `memory.c` |
| Memory map user pages (mmap_user/munmap_user) | ✅ | `memory.c` |
| TSS + enter_ring3() via iretq (ring 0 → ring 3) | ✅ | `ring3.c`, `ring3.h` |
| GDT: ring 3 code (0x18, DPL=3) + data (0x20, DPL=3) + TSS slot (0x28) | ✅ | `boot64.asm` |
| Paginação com User bit em PML4E/PDPTE/PDE (0x87) | ✅ | `boot64.asm` |
| SYS_exit detectado no handler, pula iretq, retorna | ✅ | `syscall_entry.asm` |
| Mach-O 64-bit loader (MH_MAGIC_64, LC_SEGMENT_64, LC_MAIN) | ✅ | `mach_o.c` |
| Dynamic linker (dyld: resolve, bind, load dylibs) | ✅ | `dyld.c` |
| SMC stub, NVRAM stub | 🔸 | `smc.c`, `nvram.c` |

### 1.2 Drivers

| Componente | Status | Arquivo |
|------------|--------|---------|
| VGA text mode 80x25 + cursor + scroll | ✅ | `kernel.c` |
| ANSI/VT100 escape parser (CSI, clear, reverse, cursor, hide/show) | ✅ | `kernel.c` (novo) |
| PS/2 keyboard (scancode→ASCII, shift, extended arrows/Fn) | ✅ | `keyboard.c` |
| Keyboard circular buffer + blocking read + polling fallback | ✅ | `keyboard.c` |
| VGA graphics 320x200x256 (mode 13h-style) | ✅ | `vga_gfx.c` |
| VESA framebuffer (1024×768 32-bit, init via Multiboot2 tag) | ✅ | `vesa.c` |
| Termina framebuffer nativo (fb_buf, fb_render_cell, scroll atômico) | ✅ | `kernel.c`, `vesa.c` |
| Renderização atômica (temp buffer + memcpy, sem flicker por célula) | ✅ | `vesa.c` (vesa_draw_cell) |
| Compositor: backbuffer VESA (flicker-free), 8 janelas, panel botão [+], drag, close, focus cycle, Ctrl+N/Ctrl+Q (fallback VGA 320x200) | ✅ | `compositor.c` |

### 1.3 Shell & Comandos

| Componente | Status | Função |
|------------|--------|--------|
| Shell loop "MkM> " prompt | ✅ | `shell_loop()` |
| `help` | ✅ | `cmd_help` |
| `clear` | ✅ | `cmd_clear` |
| `echo` | ✅ | `cmd_echo` |
| `about` | ✅ | `cmd_about` |
| `shutdown` | ✅ | `cmd_shutdown` |
| `ls` (FAT32 dir listing) | ✅ | `cmd_ls` |
| `touch` | ✅ | `cmd_touch` |
| `rm` | ✅ | `cmd_rm` |
| `cat` | ✅ | `cmd_cat` |
| `edit` (mini line editor) | ✅ | `cmd_edit` |
| `mkdir` | ✅ | `cmd_mkdir` |
| `cd` | ✅ | `cmd_cd` |
| `pwd` (walk FAT tree) | ✅ | `cmd_pwd` |
| `mv` (rename) | ✅ | `cmd_mv` |
| `cp` (copy sectors) | ✅ | `cmd_cp` |
| `rmdir` | ✅ | `cmd_rmdir` |
| `stat` | ✅ | `cmd_stat` |
| `disp` (VGA graphics mode) | ✅ | `cmd_disp` |
| `exec` (Mach-O loader + PATH search) | ✅ | `cmd_exec` |
| Auto-search /BIN/ for executables | ✅ | `cmd_exec` (PATH: "", BIN, APPS) |

### 1.4 Userland / libc

| Componente | Status | Arquivo |
|------------|--------|---------|
| CRT0 (_start → main → exit) | ✅ | `crt0.c` |
| `open`, `close`, `read`, `write`, `lseek` | ✅ | `stdio.c` |
| `unlink`, `mkdir`, `rmdir` | ✅ | `stdio.c` |
| `stat`, `fstat` | ✅ | `stdio.c` |
| `kbhit` | ✅ | `stdio.c` |
| `printf` (%d %u %x %s %c) | ✅ | `stdio.c` |
| `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fputs` | ✅ | `stdio.c` |
| `vsnprintf`, `sprintf` | ✅ | `stdio.c` |
| `malloc` (64KB bump), `free` (no-op) | 🔸 | `stdlib.c` |
| `atoi`, `itoa`, `exit` | ✅ | `stdlib.c` |
| `strlen`, `strcmp`, `strncpy`, `strcpy`, `strcat`, `strchr`, `strrchr`, `strstr`, `strtok` | ✅ | `string.c` |
| `memset`, `memcpy`, `memmove` | ✅ | `string.c` |
| `ctype.h` (isspace, isdigit, isalpha, isprint, etc) | ✅ | `ctype.h` |

### 1.5 Programs

| Componente | Status | Arquivo |
|------------|--------|---------|
| **graphy** — TUI text editor (64KB, 4096 linhas, find, status bar, ^O/^X/^G/^F/^C) | ✅ | `progs/graphy.c` |
| macho_pack.py — empacota binário em Mach-O | ✅ | `tools/macho_pack.py` |
| Build pipeline C→ELF→bin→Mach-O→disk | ✅ | `userland/Makefile` |

### 1.6 Build & Deploy

| Componente | Status |
|------------|--------|
| Top-level Makefile (kernel + userland + ISO + run + clean) | ✅ |
| GRUB config (Multiboot2, timeout=0) | ✅ |
| 64MB FAT32 disk.img | ✅ |
| QEMU run (TipOS.iso + disk.img + serial) | ✅ |
| QEMU run-curses | ✅ |
| `make run` → boots in <5s | ✅ |

---

## 🎯 2. FASE CRÍTICA — TORNAR USÁVEL (Semanas 1-2)

*Sem isso, o sistema é brinquedo. Com isso, já dá pra editar texto e compilar.*

### 2.1 Estabilidade Base

- [x] 📌 **sys_exit real**: limpar FD table, liberar mmap, voltar ao shell sem crash
- [x] 📌 **Keyboard repeat rate**: segurar tecla repete após delay inicial (via PIT/RTC)
- [x] **Proteger contra buffer overflow**: graphy 64KB hardcoded, strcpy sem bounds
- [x] **Verificar leaks**: kmalloc nunca libera, bump heap do userland nunca reusa
- [x] **Error handling FAT32**: o que acontece se disco enche? Se setor corrompe?
- [x] 📌 **Graceful shutdown**: sinfilar desmontar FAT32 antes de desligar

### 2.2 Editor de Texto (graphy → mature)

- [x] 📌 **Syntax highlighting** (C keywords, strings, comments, numbers) — cores ANSI
- [x] **Multiple file buffers** (^O alterna entre arquivos abertos)
- [x] **Undo/Redo** (pelo menos single-level = Ctrl+Z)
- [x] **Line numbers toggle** (F2)
- [ ] **Word wrap** (opcional, padrão off)
- [x] **Auto-indent** (manter indentação da linha anterior)
- [x] **Clipboard/cut-copy-paste** (^W cut, ^Y paste, ^K kill line)
- [x] **Go to line** (^J)
- [x] **Replace** (^R)
- [x] **Status bar melhor**: nome arquivo, modified*, line:col, encoding, mode (INS/OVR)
- [ ] **Mouse support** via terminal (X10 mode) — clicar pra posicionar cursor

### 2.3 Terminal / Shell

- [x] 📌 **Shell history** (setas ↑↓, salvo em RAM, 100+ entries)
- [x] 📌 **Line editing** (Home, End, Del, Ctrl+U/Ctrl+K/Ctrl+W, Ctrl+A/Ctrl+E, Ctrl+L limpa tela)
- [x] 📌 **PATH search** (/BIN/:/USR/BIN/:/LOCAL/BIN/ etc)
- [x] 📌 **Redirecionamento** `>` `>>` (pipe via buffer em RAM — pendente `<` `2>` `|`)
- [x] **Autocomplete** (TAB → lista arquivos, comandos)
- [x] **Prompt customizável** (PS1: `\u`, `\h`, `\w`, `\$`, etc)
- [x] **Aliases** (alias ll='ls -l')
- [x] **Background jobs** (&, jobs, fg, bg)
- [x] **Variáveis de ambiente** ($PATH, $HOME, $EDITOR)
- [x] **Scripting** — executar lista de comandos de um arquivo .sh
- [x] **^C interrompe comando atual** (precisa de sinais)

### 2.4 Relógio / Data

- [x] 📌 **Driver RTC (CMOS)** — ler hora/data real
- [x] **Syscall gettimeofday** — retornar tempo real
- [x] **Comando `date`** — mostrar data/hora
- [x] **Timestamps em arquivos** — FAT32 já tem, mas não estávamos expondo
- [x] **Sleep** — syscall usleep/sleep (via PIT ou RTC)

### 2.5 Compilador Onboard

- [x] 📌 **Bridge `cc` + `cc-host.sh`** — builtin copia .c para /SRC/, script host compila e escreve .macho em /BIN/
- [x] **Comando `make`** — build automation (Makefile parser, dependencias, timestamps, execucao de comandos)
- [ ] **Comando `as`** — assembler básico (nasm ou as)

---

## 🏗️ 3. ARQUITETURA — MULTITAREFA E ESTABILIDADE (Semanas 3-6)

*Essa fase tira o OS de "brinquedo de boot" para "plataforma séria".*

### 3.1 Proteção de Memória (RING 3) ⚠️

- [x] 📌 **TSS + mudança de anel** (ring 0 → ring 3 via `iretq` com frame CS=0x1B/RPL=3; `syscall`/`sysret` pendente para performance)
- [x] 📌 **Pilha de kernel separada por processo** (TSS.RSP0 por execução, alocada/freed via page allocator)
- [x] 📌 **Paginação por processo** (cada execução tem seu PML4 — cópia do kernel, switch cr3 na entrada/saída)
- [ ] 📌 **Syscall gate via `syscall`/`sysret`** (mais rápido que int 0x80, já ativa ring)
- [ ] **Isolar kernel em página alta** (0xFFFF800000000000+)
- [x] **TLB flush na troca de processo** (implícito no `mov cr3`)
- [ ] **Copy-on-write para fork**

- [ ] 📌 **Pilha de kernel separada por processo** — cada execução de ring 3 ganha kernel stack via page allocator, TSS.RSP0 setado por processo ← agora gerenciado pelo scheduler
- [x] 📌 **Paginação por processo** (cada execução tem seu PML4 — cópia do kernel, switch cr3 na entrada/saída)
- [ ] 📌 **Syscall gate via `syscall`/`sysret`** (mais rápido que int 0x80, já ativa ring)
- [ ] **Isolar kernel em página alta** (0xFFFF800000000000+)
- [x] **TLB flush na troca de processo** (implícito no `mov cr3`)
- [ ] **Copy-on-write para fork**

### 3.2 Processos e Scheduler

- [x] 📌 **PCB (Process Control Block)** — struct com PID, estado, kernel stack, PML4, registradores salvos
- [x] 📌 **Tabela de processos** (64 slots, alocação linear de PID)
- [x] 📌 **Scheduler preemptivo** (round-robin via PIT IRQ0, context_switch assembly, pop+iretq)
- [x] 📌 **Syscall `exit` real** — `proc_exit()` define ZOMBIE, acorda parent, schedule()
- [x] 📌 **Syscall `waitpid`** — `proc_waitpid()` bloqueia até child ZOMBIE, limpa recursos
- [ ] 📌 **Syscall `fork`** — duplicar processo
- [ ] 📌 **Syscall `execve`** — substituir processo por novo binário
- [ ] **Syscall `yield`** — voluntariamente ceder CPU
- [ ] **Syscall `getpid`** — retornar PID real
- [ ] **Syscall `kill`** — enviar sinal para processo
- [ ] **Prioridades** — nice, scheduling classes

### 3.3 Terminais Múltiplos (TUI Multiplexado 💡)

- [ ] 📌 **Abstração de terminal** — struct terminal com buffer de saída, input, scrollback
- [ ] **Múltiplos terminais** — alternar via F1-F6 (como /dev/tty1-6 no Linux)
- [ ] **Scrollback** — buffer circular de 1000+ linhas, PageUp/PageDown scrolla
- [ ] **Tabs** — Ctrl+Tab/N+Tab alterna entre sessões
- [ ] **Split horizontal/vertical** — dividir tela em painéis (tipo tmux)
- [ ] **Copy mode** — selecionar texto do scrollback, copiar para clipboard

### 3.4 Clipboard / Yank Buffer

- [ ] **Clipboard global** — ^C copia, ^V cola (entre programas)
- [ ] **Clipboard de terminal** — selecionar com mouse, copiar
- [ ] **Yank buffer** — Alt+Click cola conteúdo do buffer

### 3.5 Sinais (para matar processos de verdade)

- [ ] **SIGKILL** — mata processo
- [ ] **SIGTERM** — pede pra terminar
- [ ] **SIGINT** — ^C no terminal
- [ ] **SIGSEGV** — page fault (precisa de ring 3 primeiro)
- [ ] **signal() / sigaction()** — reais (hoje são stubs)

---

## 🦀 13. INTEGRAÇÃO RUST NO KERNEL

*Adicionar Rust como linguagem de kernel, compilando para freestanding x86_64 e linkando com o C existente.*

### 13.1 Toolchain & Target

- [x] **Instalar Rust nightly** (componentes `rust-src`, `llvm-tools`)
- [x] **Criar crate + Cargo.toml** em `src/rust/` com `crate-type = ["staticlib"]`
- [x] **Configurar `.cargo/config.toml`** com target `x86_64-unknown-none`, rustflags (`-C no-red-zone`, `-C target-feature=-mmx,-sse`)
- [x] **Integrar `cargo build` no Makefile** — crate Rust compila como `.a` e linka no `kernel.elf`

### 13.2 Rust Runtime Mínimo

- [x] **Crate `#![no_std]` + `#![no_main]`** — sem libstd, sem main
- [x] **Panic handler** — `#[panic_handler]` que chama `serial_puts` + loop `hlt`
- [x] **`panic = "abort"`** no Cargo.toml (profile.release)
- [x] **`alloc_error_handler`** — handler para alocação
- [x] **build-std = ["core", "alloc"]** — compila core + alloc do source (nightly)

### 13.3 FFI com C (Interface Externa)

- [x] **Declarar funções C como `extern "C"`** no Rust:
  - `serial_puts`, `serial_putc`
  - `kmalloc`, `kfree`
- [ ] **Cabeçalhos compartilhados** — structs C traduzidas para `#[repr(C)]` no Rust:
  - ([futuro] framebuffer_t, window_t, process_t, etc.)
- [x] **Chamar Rust de C** — `extern "C" fn rust_entry()` exportada, chamada do `kmain` via boot selector

### 13.4 Global Allocator para Rust

- [x] **TiposAllocator** implementando `GlobalAlloc` — wrappa `kmalloc`/`kfree` do C
- [x] **`#[global_allocator]`** setado em `lib.rs` para que `Box`, `Vec`, `String` funcionem
- [x] **Testar allocator** — `rust_entry()` aloca `u64` via `alloc(Layout::new::<u64>())`, testa write/read

### 13.5 Boot Selector (C vs Rust)

- [x] **Boot selector no kmain** — prompt serial + VGA: "Press [R] for Rust, [C] for C"
- [x] **Timeout 5s** — default C
- [x] **Tecla R** — chama `rust_entry()`, depois continua boot normal
- [x] **Tecla C** — pula Rust, continua boot normal

### 13.6 Próximos Passos

- [ ] **Reverter componente em Rust** — reescrever `vesa_fill_screen` + `vesa_flush` em Rust
- [ ] **Benchmark** — comparar performance Rust vs C
- [ ] **Comando `disp_rust`** — versão do compositor em Rust chamando `extern "C"` para VESA

### 13.6 Rust no Userland

- [ ] **Target `x86_64-tipos-user.json`** — para compilar programas Rust userland
- [ ] **libc bindings** — syscalls via `extern "C"` + `int 0x80` inline asm
- [ ] **Hello world Rust** — programa userland em Rust que faz `write(1, "Hello\n", 6)`
- [ ] **Port do graphy para Rust** — ou reescrever TUI library em Rust

### 13.7 Build Pipeline

- [ ] **Multi-stage link** — Rust `.o` + C `.o` + ASM `.o` → linker.ld final
- [ ] **LTO entre C e Rust** — `-C lto=fat` no Rust + `-flto` no GCC
- [ ] **Caching** — `target/` separado por build type, integrado ao `make`
- [ ] **CI com Rust** — verificar que `cargo build` + `make` produzem ISO bootável

### 13.8 Dependências e Crates (só se necessário)

- [ ] **`x86_64` crate** — tipos para paginação, port IO, MSRs, CPUID
- [ ] **`uart_16550`** — driver serial em Rust
- [ ] **`spin`** — `Mutex` spinlock para dados compartilhados
- [ ] **`linked_list_allocator`** — allocator alternativo ao bump

---

## 🛠️ 4. FERRAMENTAS DE DESENVOLVIMENTO (Meses 2-3)

*O que faz programadores sérios se interessarem.*

### 4.1 Version Control (Git 💡)

- [ ] **Port do Git** (ou implementar subset: init, add, commit, log, diff, branch, checkout, push, pull)
- [ ] **Comandos nativos**: `git init`, `git add`, `git commit`, `git status`
- [ ] **Diff highlighting** colorido no terminal
- [ ] **Integração com editor**: graphy mostra git blame, gutter com modified/added
- [ ] **.gitignore** suporte básico

### 4.2 Build System

- [ ] **`make`** — port do GNU Make ou implementação minimalista
- [ ] **`patch`** — aplicar patches
- [ ] **`diff`** — comparar arquivos
- [ ] **`install`** — copiar binários para PATH

### 4.3 Debugger

- [ ] 💡 **Debugger TUI** (tipo gdb TUI ou lldb)
- [ ] **Breakpoints**, step into/over, print variables
- [ ] **Backtrace** com símbolos
- [ ] **Attach em processo rodando**
- [ ] **Disassemble** — ver assembly em tempo real
- [ ] **Memory watch** — monitorar regiões de memória

### 4.4 Search & Navigation

- [ ] **`grep`** — busca em arquivos (com regex) 💡
- [ ] **`find`** — busca por nome/tipo/data no FS
- [ ] **`locate`** — índice rápido de arquivos
- [ ] **Fuzzy find no shell** — Ctrl+R busca history, Ctrl+T busca arquivos (tipo fzf) 💡
- [ ] **Tag system** — ctags/ctags para navegação em código 💡

### 4.5 Project Management

- [ ] **`proj`** — comando nativo que abre um "workspace"
  - Lista arquivos do projeto
  - Abre editor nos arquivos relevantes
  - Compila com um comando
  - Mostra erros em janela separada (quickfix list)
- [ ] **Split editor**: metade tela código, metade terminal/compiler output
- [ ] **Quickfix**: navegar entre erros de compilação com atalho

### 4.6 Linguagens & Toolchain

- [ ] **C** — GCC ou TCC onboard (auto-hospedagem)
- [ ] **Assembly** — nasm onboard
- [ ] **Shell script** — interpretador .sh básico
- [ ] **Python** — port do CPython minimal (micro-python?) 💡
- [ ] **Lua** — port da linguagem (embedding natural) 💡
- [ ] **Linker** — ld onboard ou script próprio

---

## 🔐 5. CORPORATE / SEGURANÇA (Meses 3-4)

*O que separa um brinquedo de um sistema corporativo.*

### 5.1 Autenticação e Usuários

- [ ] **Login** — tela de login após boot
- [ ] **Múltiplos usuários** — /etc/passwd, /etc/shadow
- [ ] **`su`** / **`sudo`** — alternar usuário
- [ ] **`whoami`**, **`id`**, **`passwd`**
- [ ] **Home directories** — /home/$USER
- [ ] **Umask / permissões** — rwx real, não stub

### 5.2 Filesystem Corporativo

- [ ] **VFS layer** — mount, umount, múltiplos FS
- [ ] **ext2** — leitura/escrita (mais robusto que FAT32 para sistema)
- [ ] **Journaling** — log de operações do FS (evitar corrupção em queda de energia)
- [ ] **Quotas** — limite de disco por usuário
- [ ] **Backup automático** — snapshots periódicos para partição de backup
- [ ] **`fsck`** — verificar e reparar sistema de arquivos no boot
- [ ] **Encryption** — criptografia de partição (AES básico)

### 5.3 Networking & Remote

- [ ] 📌 **Driver NIC** — Intel e1000 (mínimo para QEMU)
- [ ] 📌 **TCP/IP stack** — lwIP portado como servidor user-space
- [ ] **`ping`** — teste de conectividade
- [ ] **`wget`** / **`curl`** — download HTTP
- [ ] **SSH client** — port do Dropbear ou libssh
- [ ] **SSH server** — acesso remoto ao OS
- [ ] **Git over SSH/HTTPS** — clone, push, pull
- [ ] **DNS resolver** — /etc/resolv.conf
- [ ] **DHCP client** — IP automático
- [ ] **Firewall** — regras básicas de filtro

### 5.4 Logs & Monitoramento

- [ ] **syslog** — logging centralizado de kernel + apps
- [ ] **`dmesg`** — ver logs do kernel
- [ ] **`top`** / **`htop`** — monitor de processos 💡
- [ ] **`ps`** — listar processos
- [ ] **`uptime`** — tempo desde boot
- [ ] **Audit trail** — log de comandos executados por usuário

---

## 💻 6. TERMINAL & USER EXPERIENCE (Meses 3-5)

*O que faz o sistema ser PRAZEROSO de usar no dia-a-dia.*

### 6.1 Terminal Emulator Avançado

- [ ] **24-bit color support** (true color) 💡
- [ ] **UTF-8** (hoje só ASCII)
- [ ] **True-type font rendering** (bitmap fonts primeiro, depois TTF)
- [ ] **256-color palette** (hoje só 16 cores VGA)
- [ ] **Hyperlinks** (OSC 8 — clicar em URL/file:line) 💡
- [ ] **Image preview** (sixel ou kitty protocol) 💡
- [ ] **Config file** (~/.tiposrc) — keybindings, cores, fontes

### 6.2 Tiling Window Manager 💡

- [ ] **Tiling automático** — novo terminal ocupa espaço disponível
- [ ] **Atalhos**: Alt+Enter novo terminal, Alt+W fecha, Alt+setas foco, Alt+Shift+setas move
- [ ] **Modo stack** — janelas empilhadas (tabbed)
- [ ] **Resize** — Alt+Click borda, ou atalho
- [ ] **Workspaces** — múltiplos desktops virtuais (F1-F6 muda)
- [ ] **Scratchpad** — terminal flutuante com atalho global

### 6.3 Notifications

- [ ] **Barra de status** (topo ou base): relógio, sessões, load, bateria? 💡
- [ ] **Notificações** de eventos (compilação terminou, job completou)
- [ ] **Alertas visuais** — piscar tela ou flash na barra

### 6.4 Input Methods

- [ ] **Layout ABNT2** (teclado brasileiro)
- [ ] **Caps Lock** (hoje não tratado)
- [ ] **Compose key** (caracteres acentuados)
- [ ] **Keybinding customizável** — usuário define atalhos
- [ ] **Macros de teclado** — gravar/reproduzir sequência de teclas

---

## 🧩 7. SISTEMA DE ARQUIVOS & DADOS (Meses 4-6)

### 7.1 VFS Architecture

- [ ] **Mount table** — /dev/sda1 → /, /dev/sda2 → /home
- [ ] **`mount`** / **`umount`** commands
- [ ] **`fstab`** — montagens automáticas no boot
- [ ] **tmpfs** — /tmp em RAM
- [ ] **devfs** — /dev/ entries automáticos
- [ ] **procfs** — /proc/[pid]/ (info de processo)

### 7.2 FAT32 → ext2

- [ ] **ext2 driver** (read/write, symlinks, permissões)
- [ ] **Migrar partição de sistema para ext2**
- [ ] **Manter FAT32 para compatibilidade com outros SOs** (partição compartilhada)

### 7.3 Backup & Recovery

- [ ] **Snapshot tool** — salvar estado do FS em momento específico
- [ ] **`rsync`**-like — sincronizar diretórios
- [ ] **Recovery mode** — boot com opção de reparo

---

## 🌐 8. NETWORKING & COLABORAÇÃO (Meses 4-6)

### 8.1 Core Network

- [ ] **TCP/IP stack** (lwIP) funcional
- [ ] **`loopback`** (127.0.0.1)
- [ ] **Ethernet driver** (e1000, RTL8139)
- [ ] **DHCP** — configuração automática de IP

### 8.2 Remote & Sync

- [ ] **SSH client** (libssh ou port do OpenSSH)
- [ ] **SCP** — cópia remota de arquivos
- [ ] **NFS client** — montar diretório remoto
- [ ] **Git sobre HTTP/SSH** — funcional

### 8.3 Collaboration Features 💡

- [ ] **Terminal compartilhado** — múltiplos usuários veem mesma sessão (pair programming)
- [ ] **Chat integrado** — mensagens entre usuários do sistema
- [ ] **Shared clipboard** entre usuários na rede

---

## 🚀 9. SONHOS / DIFERENCIAIS (6+ meses)

*Recursos que fariam o TipOS ser único.*

### 9.1 IDE Nativa Integrada 💡

- [ ] **`proj`** — workspace manager nativo
  - Abre projeto com estrutura de diretórios
  - LSP client (Language Server Protocol) para completação de código 💡
  - Syntax highlighting em tempo real
  - Compilação com um atalho (F5)
  - Erros aparecem em painel separado, navegáveis
  - Integração com git (blame, diff inline)
- [ ] **Debugger visual** integrado (breakpoints na gutter do editor)
- [ ] **Test runner** — roda testes, mostra resultados coloridos

### 9.2 Package Manager 💡

- [ ] **`tipkg`** — instalador de pacotes nativo
  - Repositório remoto
  - Dependências
  - Versões
  - Atualização (`tipkg update && tipkg upgrade`)

### 9.3 Hypervisor 💡

- [ ] **Virtualização nativa** (VT-x/AMD-V)
- [ ] **Rodar Linux como VM convidada** com pass-through de GPU
- [ ] **`/vm windows.iso`** — rodar Windows dentro do TipOS

### 9.4 Multi-arch

- [ ] **ELF loader** — rodar binários Linux estáticos
- [ ] **Syscall translation layer** — traduzir syscalls Linux para nativas
- [ ] **Rodar busybox**, depois bash, depois GCC do Linux

### 9.5 System Recovery

- [ ] **Modo rescue** — boot com kernel mínimo + shell para reparo
- [ ] **`fsck` interativo** — reparar sistema de arquivos corrompido
- [ ] **Rollback de atualizações** — voltar versão anterior do sistema

### 9.6 Live USB

- [ ] **Modo live** — boot sem instalar, com persistência opcional
- [ ] **Instalador no live** — particiona disco, copia sistema, configura boot

---

## 📋 10. MATRIZ DE DEPENDÊNCIAS

```
Ring 3 / Proteção ───────────┬────────────────────────────┐
                              │                            │
                    Scheduler preemptivo          Sinais reais
                              │                            │
                    fork/exec/wait                  kill/^C
                              │
                    ┌─────────┴─────────┐
                    │                   │
            Múltiplos processos     Tabs/Terminais
                    │                   │
            IPC / Pipes              Scrollback
                    │                   │
            Clipboard global       Split panes
                    │
            Rede (TCP/IP)
                    │
            Git / SSH
                    │
            Colaboração
```

---

## 🏆 11. MARCOS (Versões)

| Versão | Foco | Previsão |
|--------|------|----------|
| **v0.5** | Editor maduro + history + PATH + line editing + RTC + autocomplete + env/alias + PS1 + timestamps | 2 semanas |
| **v0.6** | TCC onboard + make + date/sleep + syntax highlight | 4 semanas |
| **v0.7.0** | Ring 3 + userland CPL=3 + Mach-O loader + TSS | 8 semanas |
| **v0.7.1** | Scheduler preemptivo + PCB + paginação por processo + TLB flush | 8 semanas |
| **v0.7.2** | Backbuffer VESA (flicker-free) + panel + multi-janela (drag/close/focus/cycle) + Ctrl+N/Ctrl+Q + HEAP 64MB + RAM 512M | 8 semanas |
| **v0.8** | Terminal multiplexado (tabs/split/scrollback) + clipboard | 12 semanas |
| **v0.9** | Rede (TCP/IP + SSH + Git) | 20 semanas |
| **v1.0** | Auto-hospedagem: compilar TipOS dentro do TipOS | 24 semanas |
| **v2.0** | ext2, VFS, mount, permissões, login, corporate ready | 36 semanas |

---

## ✅ CONTRACHEQUE — O QUE JÁ FOI FEITO HOJE

*Para não perder de vista o progresso real:*

| Data | O quê |
|------|-------|
| Já | Boot + long mode funcional |
| Já | FAT32 completo (mk/rm/read/write/cd/ls/stat/mv/cp) |
| Já | 30 syscalls via int 0x80 (XNU convention) |
| Já | Teclado PS/2 com setas e extended |
| Já | ANSI escape parser no VGA (TUI real) |
| Já | Mach-O loader + dyld |
| Já | libc userland (printf, fopen, string, malloc) |
| Já | graphy — editor TUI completo |
| Já | 19 comandos shell + auto-search /BIN/ |
| Já | Compositor gráfico 320x200 |
| Já | Pipe build C→Mach-O + install no disk.img |
| HOJE | Tutorial completo (docs/tipos-tutorial.md) |
| HOJE | **RTC driver** (CMOS, gettimeofday real, date, uptime) |
| HOJE | **PIT timer** (100Hz, timer_ticks, sleep_ms) |
| HOJE | **Keyboard repeat** (500ms delay, 33Hz rate, make/break tracking) |
| HOJE | **Shell history** (128 entries circular, ↑↓ browse) |
| HOJE | **Shell line editing** (Home/End/Del, ^A/^E/^K/^U/^W/^L) |
| HOJE | **Shell PATH search** (/BIN/, /USR/BIN/, /LOCAL/BIN/) |
| HOJE | **Redirection** `>` (write) e `>>` (append) |
| HOJE | **graphy syntax highlighting** (C keywords/strings/comments/numbers) |
| HOJE | **graphy undo/redo** (512 operations) |
| HOJE | **graphy clipboard** (^W cut word, ^Y paste, ^K kill line) |
| HOJE | **graphy go to line** (^J) |
| HOJE | **graphy replace** (^R, bulk) |
| HOJE | **graphy auto-indent** (replicate indent on Enter) |
| HOJE | **graphy toggle buffer** (^T, 2 buffers) |
| HOJE | **graphy syntax-highlighted status bar** |
| HOJE | KERNEL.md — documentação nível bizarro
| HOJE | **Keyboard ISR vs polling fix** — elimina duplicação de caracteres no teclado
| HOJE | **Shell autocomplete (TAB)** — completar comandos (builtins + PATH) e nomes de arquivo (CWD)
| HOJE | **graphy line numbers toggle (F2)** — mostrar/esconder números de linha
| HOJE | **Variáveis de ambiente** — 32 slots, export, $PATH, $PS1
| HOJE | **Aliases** — 32 slots, alias/unalias com expansão no execute_command()
| HOJE | **Prompt customizável** ($PS1) lido a cada iteração do shell loop
| HOJE | **Timestamps em ls/stat** — FAT32 mtime/mdate, formato MM-DD HH:MM/ISO
| HOJE | **Graceful shutdown + reboot** — sync FS antes de desligar/reboot
| HOJE | **Scripting (source)** — ler arquivo .sh, executar linhas como comandos
| HOJE | **$? exit code** — last_exit_code + export para ambiente
| HOJE | **Terminal colorido** — system colors via colors.h (16 cores VGA, set_vga_color)
| HOJE | **PS1 escape expansion** — \u, \h, \w, \W, \s, \$ no prompt
| HOJE | **Diretório atual no prompt** — prompt mostra [/path]# via PS1=\[\w\]\\$ 
| HOJE | **Bugfix name_to_83** — "." e ".." corrompidos por detecção de ponto como separador ext
| HOJE | **Bugfix fat32_get_cwd_name** — ".." com cluster=0 (FAT32 spec: "pai é raiz") não tratado
| HOJE | **Bugfix syscall gate** — int 0x80 usava interrupt gate (IF=0 durante handler), keyboard_read() deadlockava
| HOJE | **Diagnóstico graphy** — causa raiz: syscall gate deadlockando keyboard_read() por IRQ desabilitado
| HOJE | **Freelist malloc** — libc stdlib com freelist circular + coalescência + mmap para blocos >= 2048
| HOJE | **calloc/realloc/mmap/munmap** — adicionados à libc stdlib
| HOJE | **TUI library** — tui.h/tui.c: double buffer, dirty tracking, refresh parcial, overlapped windows
| HOJE | **TUI widgets** — dialog, msgbox, prompt integrados à TUI lib
| HOJE | **vsnprintf/sprintf** reais na libc (antes stubs vazios)
| HOJE | **strdup** adicionado à libc
| HOJE | **graphy refatorado** — agora usa TUI library (sem flicker, refresh parcial)
| **HOJE** | **Ring 3 funcional** — GDT com segmentos ring3 (0x18 code, 0x20 data), TSS slot (0x28), enter_ring3() via iretq |
| **HOJE** | **User bit na paginação** — PML4E/PDPTE com +7 (User), PDE com 0x87, ring 3 acessa VGA/text |
| **HOJE** | **SYS_exit real** — syscall handler pula iretq se r15==1, retorna ao ring 0 com rsp salvo |
| **HOJE** | **TUI userland com syscalls** — graphy em ring 3 usa write/read/exit via int 0x80 |
| **HOJE** | **PATH search no exec** — cmd_exec busca "", "BIN", "APPS" automaticamente |
| **HOJE** | **ATA timeout** — polling loop com 100k iterações, não trava se primary master vazio |
| **HOJE** | **Makefile corrigido** — disk.img como ATA primary master (`-drive file=... -boot order=d`) |
| **HOJE** | **crt0 exit** — _start() chama exit(ret) no final (syscall SYS_exit) |
| **HOJE** | **Commit + push GitHub** — d1741b0 no TipGroup-inc/TipOS-staging.git |
| **HOJE** | **munmap_all_user() + fds_cleanup() reais no SYS_exit** — limpa todo bitmap + FD table ao sair de ring 3 |
| **HOJE** | **Background jobs** — parsing de `&`, job table (16 slots), builtins `jobs`/`fg`/`bg` |
| **HOJE** | **Makefile raiz delegado p/ OvsbMkM/** — elimina duplicação de regras |
| **HOJE** | **READMEs atualizados** — raiz, OvsbMkM/, userland/ com ring3, graphy, 30 syscalls |
| **HOJE** | **^C (SIGINT) interrompe ring 3** — sigint_pending flag, abort via syscall_entry.asm |
| **HOJE** | **Ctrl modifier no keyboard** — ctrl_pressed separado de shift, Ctrl+letra → control codes |
| **HOJE** | **graphy buffer overflow protection** — strncpy+null, clamp memcpy to COLS, grow overflow check, file size limit |
| **HOJE** | **kfree no-op** — bump allocator não pode free, kmalloc não usado; previne corrupção do bitmap |
| **HOJE** | **FAT32 error handling** — checks em mkdir/write_chain/alloc_clusters/write_file, FAT_ERR_NOTEMPTY |
| **HOJE** | **cc builtin + cc-host.sh** — `cc <arquivo>` copia fonte para /SRC/; `cc-host.sh <nome>` extrai, compila com toolchain do userland, e escreve .macho em /BIN/ |
| **HOJE** | **make builtin** — Makefile parser, deps, timestamps, execucao de comandos via execute_command |
| **HOJE** | **Pilha de kernel separada por processo** — TSS.RSP0 alocado por execução ring 3 |
| **HOJE** | **Paginação por processo** — cada execução de ring 3 ganha PML4 próprio, switch cr3 na entry/exit |
| **HOJE** | **TLB flush na troca de processo** — implícito no mov cr3 |
| **HOJE** | **PCB + tabela de processos** — 64 slots, PID, estado, kernel_rsp, pml4, regs |
| **HOJE** | **Scheduler preemptivo** — PIT IRQ0 chama schedule(), context_switch em switch.asm, pop+iretq |
| **HOJE** | **proc_spawn + proc_exit + proc_waitpid** — criação, termino e espera de processos |
| **HOJE** | **syscall exit → proc_exit** — SYS_exit chama proc_exit(), não retorna ao kernel |
| **HOJE** | **^C chama proc_exit(-1)** — sigint_pending no handler mata o processo |
| **HOJE** | **cmd_exec usa processo** — exec ring 3 via proc_spawn + proc_waitpid, não mais enter_ring3 direto |
| **v0.7.2.0** | **VESA backbuffer (vesa_set_backbuffer/vesa_flush)** — frame buffer secundário, full redraw, zero flicker |
| **v0.7.2.0** | **Panel na titlebar** — barra horizontal no topo, botão [+] verde abre nova janela |
| **v0.7.2.0** | **Multi-janela** — 8 janelas simultâneas, cascata, z-order, close [X] vermelho |
| **v0.7.2.0** | **Window drag** — Space+WASD move janela + cursor (modo arrasto na titlebar) |
| **v0.7.2.0** | **Focus cycle** — Tab cicla foco entre janelas, click no panel foca |
| **v0.7.2.0** | **Ctrl+N / Ctrl+Q** — atalhos de teclado para nova janela / fechar focada |
| **v0.7.2.0** | **Cursor 8x8 quadrado** — outline white, fill black, center white dot |
| **v0.7.2.0** | **HEAP 64MB** — bump allocator estendido de 4MB para 64MB (0x900000-0x4900000) |
| **v0.7.2.0** | **RAM 512MB** — QEMU -m 256M → -m 512M |
| **v0.7.2.0** | **Bugfix backbuffer** — mmap_user (não kmalloc) para backbuffer, evitava page fault loop por corrupção de page tables |
| **v0.7.3.0** | **disp-wm separado** — WM extraído para repositório próprio (`disp-wm/`), kernel expõe syscalls 200 (disp_get_fb) e 201 (disp_flush), cmd_disp exec disp-wm.macho como userland ring 3 |
| **v0.7.3.0** | **lseek movido para syscall 202** — desocupa 200/201 para as syscalls de display |
| **v0.7.3.0** | **ATA fix (BSY polling)** — wait BSY==0 antes de enviar comando ATA, polling `BSY==0 && DRQ==1` evita hangs |
| **v0.7.3.0** | **Integração Rust** — crate `src/rust/` compila como staticlib (nightly, `x86_64-unknown-none`), linkada no kernel. GlobalAlloc wrappa `kmalloc`/`kfree` do C |
| **v0.7.3.0** | **Boot selector** — prompt C/R na inicialização, timeout 5s default C. VGA + serial output |

---

## 🔮 12. VISÃO DE FUTURO — TUI Estado da Arte

*Recursos que dependem de melhorias futuras no kernel (ring 3, scheduler, driver de vídeo, mouse, sinais).*

### 12.1 Renderização
- **256 cores / true color** — requer VGA graphics mode ou framebuffer com suporte a 24-bit
- **Sixel / Kitty Graphics Protocol** — exibir imagens no terminal (requer framebuffer)
- **Alpha blending / Z-buffer** — composição com transparência (requer framebuffer + aceleração)
- **Animação / palette fades** — transições suaves entre estados

### 12.2 Input
- **Mouse** — click, drag, scroll wheel (requer driver PS/2 mouse + IRQ12)
- **Toque / gestos** — multitouch (requer hardware touch + driver)
- **Protocolo de teclado não-ambíguo** — kitty keyboard protocol (requer suporte no kernel)

### 12.3 Acessibilidade
- **Screen reader API** — anúncios contextuais para leitores de tela
- **Modo de acessibilidade** — interface simplificada com hints para elementos

### 12.4 Internacionalização
- **Unicode / UTF-8** — requer VGA text mode com fontes customizadas ou framebuffer
- **BiDi** — renderização de texto da direita pra esquerda (hebraico, árabe)
- **Emojis** — requer renderização de bitmap no terminal

### 12.5 Arquitetura
- **Sinais (SIGWINCH)** — redimensionamento automático (requer signals no kernel)
- **Multi-threading** — operações em background sem bloquear UI (requer scheduler preemptivo)
- **Serialização de sessão** — salvar/restaurar estado completo (aberto, cursor, undo)
- **Hot-reload de temas** — trocar cores em tempo real sem reiniciar app
- **Reactive state management** — hooks tipo React para gerenciamento de estado
- **Terminfo/termcap** — banco de dados de capacidades de terminal (requer FS)

### 12.6 Backlog Imediato (fazer assim que possível)
- [ ] graphy: word wrap
- [ ] graphy: scroll horizontal (linhas > 80 col)
- [x] graphy: bracket matching
- [x] shell: background jobs (&, jobs, fg, bg)
- [x] shell: ^C interrompe comando atual
- [ ] TUI lib: suporte a config file (~/.tiposrc)
- [ ] TUI lib: menu bar widget, list selector
- [ ] TUI lib: scrollback history buffer por janela

---

> *"Um sistema operacional não é sobre o que você pode fazer — é sobre o que você
> pode construir."*
