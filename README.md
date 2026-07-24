# TipOS

Sistema operacional TUI-first com kernel OvsbMk, libc própria, ring 3,
e editor gráfico TUI (graphy). Boota em QEMU, hardware real via GRUB.

```
Versão: v0.7.3.0
         STAGE  = 7 (Ring 3 + VESA framebuffer)
         RELEASE = 3 (boot selector + integração Rust)
         FEATURE = 0
```

## O que tem de legal

- **Integração Rust no kernel**: crate `#![no_std]` compilada com nightly,
  linkada como staticlib. `GlobalAlloc` wrappa `kmalloc`/`kfree` do C.
  Boot selector `[C]`/`[R]` na inicialização escolhe memory manager.
- **Ring 3 funcional**: programas userland rodam em CPL=3 com TSS, iretq,
  segmentos ring 3 (0x18/0x20), bit User na paginação
- **Editor graphy**: TUI text mode com syntax highlight C, undo/redo (512),
  clipboard (cut/copy/paste), replace, go-to-line, auto-indent, line numbers,
  busca, bracket matching, 2 buffers simultâneos
- **Shell completo**: 20+ comandos, PATH search (CWD → BIN → APPS), history
  (128), line editing (Home/End/Del/^A/^E/^K/^U/^W), autocomplete (TAB),
  redirection `>` / `>>`, aliases, env vars, PS1 customizável, scripting (source)
- **FAT32 completo**: mkdir, rmdir, rm, mv, cp, cat, edit, stat, timestamps
- **30 syscalls** via int 0x80 (convenção XNU): read/write/open/close/mmap/
  stat/fstat/lseek/kbhit/gettimeofday/sleep/exit
- **Mach-O 64-bit loader**: carrega userland programs em 0x2000000
- **RTC real**: gettimeofday, date, sleep
- **Keyboard repeat**: 500ms delay, 33Hz rate
- **Compositor gráfico (disp-wm)**: WM standalone em ring 3, repositório separado (`disp-wm/`), syscalls 200-201. 1024x768 32-bit VESA framebuffer com backbuffer (flicker-free), multi-janela (8), drag, close, focus cycle, panel com botão [+], Ctrl+N/Ctrl+Q shortcuts

## Estrutura

```
TipOS/
├── Makefile              # Build principal (kernel + Rust + ISO)
├── README.md
├── disk.img              # FAT32 (gitignored)
├── TipOS.iso             # ISO bootável (gitignored)
│
├── OvsbMk/               # Kernel (ring 0)
│   ├── src/kernel/       #   kmain, IDT, syscalls, ring3, Mach-O, disp_api
│   ├── src/drivers/      #   ATA, PS/2 keyboard, VGA text/gfx
│   ├── src/fs/           #   FAT32
│   └── os/shell/         #   Shell + builtins
│       └── os/wm/        #   Compositor (fallback VGA)
│
├── src/rust/             # Rust kernel crate
│   ├── Cargo.toml        #   staticlib, no_std
│   ├── .cargo/config.toml
│   └── src/
│       ├── lib.rs         #   rust_entry(), panic/alloc handlers
│       ├── ffi.rs         #   extern "C" declarações
│       └── allocator.rs   #   TiposAllocator (GlobalAlloc)
│
├── disp-wm/              # Window Manager (userland ring 3, repo separado)
│
├── src/userland/         # Userland MIT
│   ├── Makefile          #   .c → .macho → disk.img
│   ├── include/          #   libc headers
│   ├── libc/             #   stdio, stdlib, string, crt0, tui
│   ├── progs/            #   graphy.c e outros
│   └── tools/            #   macho_pack.py
│
├── docs/                 # Documentação
└── Discord_docc/         # Discord docs (planejamento/equipe)
```

## Build & Run

**Requisito**: Rust nightly (`rustup toolchain install nightly`, `rustup target add x86_64-unknown-none`)

```bash
# Tudo de uma vez
make all                 # Rust crate → kernel .elf → ISO
make run                 # QEMU (512M RAM, FAT32 disk)

# Ou separado
make rust                # só cargo build da crate Rust
make kernel              # só kernel .elf (inclui Rust)
make iso                 # só ISO

# QEMU manual
qemu-system-x86_64 \
    -cdrom TipOS.iso \
    -drive file=disk.img,format=raw,index=0 \
    -boot order=d \
    -m 512M \
    -serial stdio
```

## Rodar no macOS (via Docker)

O build **nativo não funciona no macOS**: o `gcc` do sistema é o Apple clang
(arm64/Mach-O), que não gera ELF x86_64, e faltam `grub-mkrescue`, `xorriso`,
`mtools`/`dosfstools` e o Rust nightly. A forma testada é **buildar dentro de um
container Linux x86_64** e **rodar o QEMU no próprio macOS** (abre janela nativa).
Nenhuma alteração no código é necessária.

**Pré-requisitos:** Docker Desktop rodando e QEMU no host (`brew install qemu`).

**1. Criar a imagem da toolchain** (uma vez). Salve como `docker/Dockerfile` ou
use o here-doc abaixo:

```bash
docker build --platform linux/amd64 -t tipos-build - <<'EOF'
FROM --platform=linux/amd64 ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/root/.cargo/bin:${PATH}"
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential nasm grub-pc-bin grub-common xorriso mtools \
      dosfstools qemu-system-x86 python3 curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
      | sh -s -- -y --default-toolchain nightly --profile minimal --component rust-src \
    && rustup target add --toolchain nightly x86_64-unknown-none
WORKDIR /work
EOF
```

**2. Buildar** (a partir da raiz do repo; os artefatos `TipOS.iso`/`disk.img`
aparecem direto no seu disco via bind-mount):

```bash
docker run --rm -it --platform linux/amd64 -v "$PWD":/work -w /work tipos-build \
  bash -lc 'make all && make disk.img && make userland'
```

**3. Rodar no QEMU do macOS** (janela gráfica):

```bash
make run     # QEMU 512M + disk FAT32 (o target run não usa -enable-kvm)
```

> No Apple Silicon, o container amd64 e o QEMU rodam por **emulação (TCG/Rosetta)**:
> funciona, mas é mais lento — não afeta a corretude. No boot selector, aperte
> **[R]** em 5s para o path Rust ou **[C]** (padrão) para o C.

## Boot Selector

Ao iniciar, o kernel exibe na tela:

```
TipOS Boot Selector
Press [R] for Rust memory manager
Press [C] for C memory manager
Default: C in 5s...
```

- **C** — continua com o memory manager C (bump allocator via `kmalloc`/`kfree`)
- **R** — chama `rust_entry()`, que inicializa o `GlobalAlloc` Rust (wrappando `kmalloc`/`kfree`) e roda um teste de alocação
- **Timeout** (5s) — padrão: C

## Shell (MkM)

| Comando   | Descrição                     |
|-----------|-------------------------------|
| `help`    | Lista comandos                |
| `clear`   | Limpa a tela                  |
| `echo`    | Imprime texto                 |
| `about`   | Sobre o sistema               |
| `ls`      | Lista diretório               |
| `touch`   | Cria arquivo                  |
| `rm`      | Remove arquivo                |
| `cat`     | Exibe arquivo                 |
| `edit`    | Edita arquivo (line editor)   |
| `mkdir`   | Cria diretório                |
| `cd`      | Muda diretório                |
| `pwd`     | Caminho atual                 |
| `mv`      | Move/renomeia                 |
| `cp`      | Copia                        |
| `rmdir`   | Remove diretório              |
| `stat`    | Info do arquivo               |
| `date`    | Data/hora                     |
| `exec`    | Executa programa (PATH search)|
| `source`  | Executa script .sh            |
| `export`  | Define variável de ambiente   |
| `alias`   | Define alias                  |

**PATH search**: `exec graphy` busca em CWD, depois BIN, depois APPS.

## Syscalls (int 0x80, XNU convention)

| Nº | Nome      | rax | rdi        | rsi        | rdx         | Descrição            |
|----|-----------|-----|------------|------------|-------------|----------------------|
| 1  | exit      | 1   | code       | -          | -           | Retorna ao shell     |
| 3  | read      | 3   | fd         | buffer     | count       | Teclado (fd=0)      |
| 4  | write     | 4   | fd         | buf        | count       | VGA (fd=1,2)        |
| 5  | open      | 5   | path       | flags      | mode        | Arquivo             |
| 6  | close     | 6   | fd         | -          | -           | Fecha fd            |
| 10 | unlink    | 10  | path       | -          | -           | Remove arquivo      |
| 20 | getpid    | 20  | -          | -          | -           | PID (sempre 1)      |
| 33 | access    | 33  | path       | mode       | -           | Verifica acesso     |
| 47 | getgid    | 47  | -          | -          | -           | GID (sempre 0)      |
| 54 | ioctl     | 54  | fd         | request    | -           | Stub (ret 0)        |
| 73 | munmap    | 73  | addr       | length     | -           | Libera mmap         |
| 74 | mprotect  | 74  | addr       | length     | prot        | Stub (ret 0)        |
| 116| gettimeofday|116| tv         | -          | -           | Timestamp           |
| 134| sigaction | 134 | signum     | act        | oldact      | Stub                |
| 136| mkdir     | 136 | path       | mode       | -           | Cria diretório      |
| 137| rmdir     | 137 | path       | -          | -           | Remove diretório    |
| 173| sigreturn | 173 | -          | -          | -           | Stub                |
| 188| stat      | 188 | path       | stat buf   | -           | Info arquivo        |
| 189| fstat     | 189 | fd         | stat buf   | -           | Info por fd         |
| 197| mmap      | 197 | addr       | length     | prot        | Aloca páginas       |
| 198| kbhit     | 198 | -          | -          | -           | Tecla disponível?   |
| 199| lstat     | 199 | path       | stat buf   | -           | Stub (igual stat)   |
| 200| disp_get_fb | 200 | -        | -          | -           | Endereço framebuffer|
| 201| disp_flush | 201 | -         | -          | -           | Flush framebuffer   |
| 202| lseek     | 202 | fd         | offset     | whence      | Posiciona em fd     |

Registradores preservados: `rbx, rbp, r12-r15`.

## Userland (MIT)

Programas em `src/userland/progs/` usam a libc TipOS:

- **stdio**: printf, fopen/fclose/fread/fwrite, getchar/putchar, kbhit, sprintf
- **stdlib**: malloc (freelist), calloc, realloc, free, atoi, itoa, exit
- **string**: memset, memcpy, memmove, strlen, strcmp, strncmp, strcpy, strncpy,
  strcat, strchr, strrchr, strstr, strtok, strdup, strtol
- **ctype**: isdigit, isspace, isalpha, isprint, etc.
- **tui**: TUI library com double buffer, refresh parcial, janelas sobrepostas,
  widgets (dialog, msgbox, prompt), cores 16 VGA, teclas estendidas

### graphy — TUI Text Editor

Editor de texto no terminal com sintaxe highlight para C, 4096 linhas,
64KB de buffer, dois buffers simultâneos (^T alterna).

| Atalho   | Função                    |
|----------|---------------------------|
| ^O       | Salvar                    |
| ^X       | Sair                      |
| ^G       | Help toggle               |
| ^F       | Buscar                    |
| ^R       | Substituir                |
| ^J       | Ir para linha             |
| ^Z       | Undo                      |
| ^W       | Cut word                  |
| ^Y       | Paste                     |
| ^K       | Kill line                 |
| ^C       | Comando (:w, :q)          |
| ^T       | Alterna buffer            |
| F2       | Line numbers toggle       |
| INS      | Insert/Overwrite toggle   |
| Setas    | Navegação                 |
| Home/End | Início/fim da linha       |
| PgUp/Dn  | Rolagem                   |

## Teclas estendidas

| Tecla      | Sequência     |
|------------|---------------|
| ↑ ↓ ← →   | `\x1b[A-D`   |
| Home       | `\x1b[H`      |
| End        | `\x1b[F`      |
| PgUp       | `\x1b[5~`    |
| PgDn       | `\x1b[6~`    |
| Insert     | `\x1b[2~`    |
| Delete     | `\x1b[3~`    |
| F1-F4      | `\x1bOP-S`   |
| F5-F8      | `\x1b[15~-18~`|
| F9-F12     | `\x1b[20~-24~`|

## Licença

- **Userland TipOS** (`src/userland/`): MIT
- **Kernel OvsbMk** (`OvsbMk/`): licença do autor original (Bugsappetit.inc)
