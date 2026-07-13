# TipOS

Sistema operacional TUI-first com kernel OvsbMkM, libc própria, ring 3,
e editor gráfico TUI (graphy). Boota em QEMU, hardware real via GRUB.

```
Versão: v0.7.1.0
         STAGE  = 7 (Ring 3 + VESA framebuffer)
         RELEASE = 1 (VESA nativo + terminal fb + compositor VESA)
         FEATURE = 0
```

## O que tem de legal

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
- **Compositor gráfico**: 1024x768 32-bit via VESA framebuffer (fallback 320x200x256 VGA), 8 janelas, cursor software

## Estrutura

```
TipOS/
├── Makefile              # Build principal (kernel + ISO)
├── README.md
├── disk.img              # FAT32 (gitignored)
├── OvsbMkM.iso           # ISO bootável (gitignored)
│
├── OvsbMkM/              # Kernel (ring 0)
│   ├── src/kernel/       #   kmain, IDT, syscalls, ring3, Mach-O
│   ├── src/drivers/      #   ATA, PS/2 keyboard, VGA text/gfx
│   ├── src/fs/           #   FAT32
│   └── src/commands/     #   Shell + builtins + compositor
│
├── src/userland/         # Userland MIT
│   ├── Makefile          #   .c → .macho → disk.img
│   ├── include/          #   libc headers
│   ├── libc/             #   stdio, stdlib, string, crt0, tui
│   ├── progs/            #   graphy.c e outros
│   └── tools/            #   macho_pack.py
│
├── docs/                 # Documentação
└── build/                # Artefatos
```

## Build & Run

```bash
# Tudo de uma vez
make                     # kernel + ISO
make -C src/userland install  # userland → disk.img
make run                 # QEMU

# Ou separado
make -C OvsbMkM          # só kernel .elf
make -C OvsbMkM iso      # gera OvsbMkM.iso
make -C src/userland install  # compila + mcopy pro disk.img

# QEMU manual
qemu-system-x86_64 \
    -cdrom OvsbMkM/OvsbMkM.iso \
    -drive file=disk.img,format=raw,index=0 \
    -boot order=d \
    -m 256M
```

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
| 200| lseek     | 200 | fd         | offset     | whence      | Posiciona em fd     |

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
- **Kernel OvsbMkM** (`OvsbMkM/`): licença do autor original (Bugsappetit.inc)
