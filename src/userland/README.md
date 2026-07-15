# TipOS Userland

Programas userland rodam em **ring 3** via `int $0x80` para syscalls.
O kernel carrega binários Mach-O 64-bit em `0x2000000` e entra em
CPL=3 via `iretq`.

## Quickstart

```bash
make install          # compila + copia pro disk.img
```

No shell do kernel: `exec GRAPHY` (PATH search automático).

## Como escrever um programa

### 1. Crie `progs/meuprog.c`

Use a libc normalmente:

```c
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Ola userland! argc=%d\n", argc);
    return 0;
}
```

O `crt0.c` chama `main(argc, argv)` e depois `exit(ret)`
(syscall SYS_exit via int 0x80).

### 2. Adicione no `Makefile`

```makefile
PROGS = graphy meuprog
```

### 3. Compile e instale

```bash
make install
```

### 4. Execute

```
[/]# exec MEUPROG
```

## Syscalls (int 0x80, XNU convention)

| #  | Nome     | rax | rdi        | rsi        | rdx         | rcx   |
|----|----------|-----|------------|------------|-------------|-------|
| 1  | exit     | 1   | code       | -          | -           | -     |
| 3  | read     | 3   | fd (0)     | buffer     | count       | -     |
| 4  | write    | 4   | fd (1,2)   | string     | count       | -     |
| 5  | open     | 5   | path       | flags      | mode        | -     |
| 6  | close    | 6   | fd         | -          | -           | -     |
| 197| mmap     | 197 | addr (0)   | length     | prot        | flags |
| 73 | munmap   | 73  | addr       | length     | -           | -     |
| 188| stat     | 188 | path       | stat buf   | -           | -     |
| 189| fstat    | 189 | fd         | stat buf   | -           | -     |
| 202| lseek    | 202 | fd         | offset     | whence      | -     |
| 198| kbhit    | 198 | -          | -          | -           | -     |
| 116| gettimeofday|116| tv         | -          | -           | -     |
| 136| mkdir    | 136 | path       | mode       | -           | -     |
| 137| rmdir    | 137 | path       | -          | -           | -     |
| 10 | unlink   | 10  | path       | -          | -           | -     |
| 33 | access   | 33  | path       | mode       | -           | -     |
| 20 | getpid   | 20  | -          | -          | -           | -     |

Registradores preservados: `rbx, rbp, r12-r15`.

## Libc

| Header     | Funções |
|------------|---------|
| `stdio.h`  | printf, sprintf, vsnprintf, fopen/fclose/fread/fwrite/fgets, getchar/putchar, kbhit |
| `stdlib.h` | malloc (freelist), calloc, realloc, free, atoi, itoa, exit |
| `string.h` | memset, memcpy, memmove, strlen, strcmp, strncmp, strcpy, strncpy, strcat, strchr, strrchr, strstr, strtok, strdup, strtol |
| `ctype.h`  | isdigit, isspace, isalpha, isprint, isalnum, isupper, islower, tolower, toupper |
| `sys/stat.h` | struct stat, stat(), fstat() |
| `tui.h`    | TUI library: init/end, windows, addch/addstr, colors, getch, refresh, widgets |

## TUI Library

A libc inclui uma TUI library (`tui.h`/`tui.c`) com:

- Double buffer com dirty tracking
- Refresh parcial (só o que mudou)
- Janelas sobrepostas (z-order)
- Cores 16 VGA
- Widgets: dialog, msgbox, prompt
- Teclas estendidas (setas, Fn, Home/End, PgUp/Dn)
- Input via `tui_getch()` (read syscall)

### graphy — TUI Text Editor

Editor completo usando a TUI library:

- Syntax highlight C (keywords, strings, comments, numbers, preprocessor)
- Undo/redo (512 operações)
- Clipboard: cut word (^W), paste (^Y), kill line (^K)
- Find (^F), Replace (^R), Go to line (^J)
- Auto-indent na quebra de linha
- Two buffers simultâneos (^T alterna)
- Line numbers toggle (F2)
- Insert/Overwrite (INS)
- Status bar com nome, modificado, linha:col, modo

## Formato binário

Mach-O 64-bit minimalista:

- **magic**: `0xFEEDFACF`
- **cputype**: `0x01000007` (x86_64)
- **filetype**: `2` (MH_EXECUTE)
- **slide base**: `0x2000000` (33 MB)
- **entry**: LC_MAIN (entryoff = offset do código)

O empacotador `tools/macho_pack.py` converte flat binary (.bin)
em Mach-O com header + LC_SEGMENT_64 __TEXT + LC_MAIN + code.

## Fluxo de build

```
meuprog.c
    │ gcc -ffreestanding -nostdlib -static -fno-PIC -e _start -T libc/link.ld
    ↓
meuprog.elf
    │ objcopy -O binary -j .text
    ↓
meuprog.bin              (flat binary, só .text)
    │ tools/macho_pack.py
    ↓
meuprog.macho            (Mach-O 64-bit)
    │ mcopy -i ../../disk.img ::/BIN/MEUPROG
    ↓
FAT32 /BIN/MEUPROG
```

## PATH

O kernel busca executáveis em:
1. CWD (diretório atual)
2. `BIN/`
3. `APPS/`

`exec graphy` funciona de qualquer diretório.
