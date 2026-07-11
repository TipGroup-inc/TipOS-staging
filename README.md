# TipOS v0.1.0.0

Sistema operacional minimalista com userland MIT, kernel OvsbMkM,
libc própria e programas TUI (editor, etc.).

```
Versão: v0.STAGE.RELEASE.FEATURE
         STAGE  = 5 (Editor maduro + RTC + timer + repeat + shell editing + history + PATH + redirect + syntax highlight + undo + clipboard)
         RELEASE = 0
         FEATURE = 0
```

## Estrutura do projeto

```
TipOS/
├── Makefile              # Build principal
├── VERSION               # Versão atual
├── README.md
├── disk.img              # FAT32 (gerado, gitignored)
├── TipOS.iso             # ISO bootável (gerado, gitignored)
│
├── OvsbMkM/              # Kernel
│   ├── src/kernel/       #   kmain, IDT, syscalls, Mach-O loader
│   ├── src/drivers/      #   ATA, PS/2 keyboard, VGA
│   ├── src/fs/           #   FAT32
│   └── src/commands/     #   Shell + builtins
│
├── src/userland/         # Userland TipOS (MIT)
│   ├── Makefile          #   Compila .c -> .macho -> instala no disco
│   ├── include/          #   Headers da libc (stdio.h, stdlib.h, ...)
│   ├── libc/             #   stdio.c, stdlib.c, string.c, crt0.c
│   ├── progs/            #   Programas exemplo (graphy.c, ...)
│   └── tools/            #   macho_pack.py
│
├── docs/                 # Documentação
│   ├── tipos-tutorial.md #   Tutorial: como fazer programas
│   └── ...
│
└── build/                # Artefatos de compilação
```

## Build & Run

```bash
make              # kernel + userland + ISO + disco (tudo)
make run          # tudo + QEMU com serial (-serial stdio)
make run-curses   # tudo + QEMU no terminal (-display curses)
make clean        # limpa artefatos
```

Os targets `run` e `run-curses` sobem QEMU com o ISO e o disco FAT32
automaticamente.

Rodagem manual (útil quando já tem o ISO):

```bash
qemu-system-x86_64 \
    -boot order=d \
    -cdrom TipOS.iso \
    -m 256M \
    -drive file=disk.img,format=raw,if=ide
```

Com KVM (mais rápido):

```bash
qemu-system-x86_64 -enable-kvm -boot order=d -cdrom TipOS.iso \
    -m 256M -drive file=disk.img,format=raw,if=ide
```

## Comandos do shell (MkM>

| Comando   | Descrição                     |
|-----------|-------------------------------|
| `help`    | Lista comandos                |
| `clear`   | Limpa a tela                  |
| `echo`    | Imprime texto                 |
| `about`   | Sobre o sistema               |
| `shutdown`| Desliga                       |
| `ls`      | Lista diretório atual         |
| `touch`   | Cria arquivo                  |
| `rm`      | Remove arquivo                |
| `cat`     | Exibe arquivo                 |
| `edit`    | Edita arquivo                 |
| `mkdir`   | Cria diretório                |
| `cd`      | Muda diretório                |
| `pwd`     | Caminho atual                 |
| `mv`      | Move/renomeia                 |
| `cp`      | Copia                        |
| `rmdir`   | Remove diretório              |
| `stat`    | Info do arquivo               |
| `exec`    | Executa programa (caminho completo) |

**Auto-busca**: se o comando não for builtin, o shell procura em `/BIN/`
automaticamente. `GRAPHY` funciona sem `exec /BIN/GRAPHY`.

## Syscalls (int 0x80, estilo XNU)

| Nº   | Nome      | Descrição                          |
|------|-----------|------------------------------------|
| 1    | exit      | Retorna ao shell                   |
| 3    | read      | Lê teclado (fd=0, blocking)        |
| 4    | write     | Escreve no VGA (fd=1,2)            |
| 5    | open      | Abre arquivo                       |
| 6    | close     | Fecha fd                           |
| 10   | unlink    | Remove arquivo                     |
| 33   | access    | Verifica acesso                    |
| 136  | mkdir     | Cria diretório                     |
| 137  | rmdir     | Remove diretório                   |
| 188  | stat      | Info do arquivo (path)             |
| 189  | fstat     | Info do arquivo (fd)               |
| 198  | kbhit     | Tecla disponível? (0/1)            |
| 200  | lseek     | Posiciona em fd                    |

Chamada: `rax=número, rdi=a1, rsi=a2, rdx=a3, rcx=a4; int $0x80`

## Userland (MIT)

Programas em `src/userland/progs/` usam a libc TipOS:
- `stdio.h` — printf, fopen/fclose/fread/fwrite, getchar/putchar, kbhit
- `stdlib.h` — malloc, free, atoi, exit
- `string.h` — memset, memcpy, strlen, strcmp, strncpy, strncmp, strchr, strstr, strtok, strtol
- `ctype.h` — isdigit, isspace, etc.
- `sys/stat.h` — struct stat, stat(), fstat()

O linker script (`libc/link.ld`) posiciona o código em `0x2000000`
e o `macho_pack.py` empacota como Mach-O 64-bit minimal.
O loader do kernel copia o binário para `0x2000000` e salta
para o entry point (`_start` em `crt0.c`).

## Teclas estendidas

O driver de teclado emite sequências VT100 para setas e
teclas especiais, interpretadas pela libc de programas como
`graphy` via `getchar()` + `kbhit()`:

| Tecla        | Sequência     |
|--------------|---------------|
| ↑ ↓ ← →     | `\x1b[A-D`    |
| Home         | `\x1b[H`      |
| End          | `\x1b[F`      |
| PgUp         | `\x1b[5~`     |
| PgDn         | `\x1b[6~`     |
| Insert       | `\x1b[2~`     |
| Delete       | `\x1b[3~`     |
| F1-F4        | `\x1bOP-S`    |
| F5-F8        | `\x1b[15~-18~`|
| F9-F12       | `\x1b[20~-24~`|

## Licença

- **Userland TipOS** (`src/userland/`): MIT
- **Kernel OvsbMkM** (`OvsbMkM/`): licença do autor original (Bugsappetit.inc)
