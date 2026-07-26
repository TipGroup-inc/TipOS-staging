/* moe moe kyun <3 */
# TipOS

Sistema operacional com kernel OvsbMkM (ring 3, VESA, OWT), libc própria,
compositor gráfico disp-wm, e editor TUI graphy. Boota em QEMU.

```
Versão: v0.7.3.0
```

## O que tem de legal

- **Ring 3 funcional**: programas userland rodam em CPL=3 com TSS, iretq
- **OWT (Ovsb Widget Toolkit)**: Label, Button, TextBox, ListView, ComboBox, Menu, Dialog, StatusBar, tema escuro/claro
- **Window Manager**: backbuffer, multi-janela, compositor VESA
- **Compositor gráfico disp-wm**: WM standalone em ring 3, 1024x768 32-bit VESA, multi-janela (8), drag, close, panel
- **Shell**: help, clear, echo, info, hexdump, run, exec, ls, cd, owt, reboot
- **FAT32**: suporte a leitura/escrita em disco
- **24 syscalls** via int 0x80: read/write/open/close/mmap/stat/lseek/kbhit/gettimeofday/exit
- **Mach-O 64-bit loader**: carrega userland programs em 0x2000000
- **Editor graphy**: TUI text mode com syntax highlight C, undo/redo

## Estrutura

```
TipOS/
├── Makefile              # Build principal (kernel + ISO + userland)
├── README.md
├── disk.img              # FAT32 (gitignored)
├── TipOS.iso             # ISO bootável (gitignored)
│
├── OvsbMk/               # Kernel (ring 0)
│   ├── kernel/           #   kmain, IDT, syscalls, process, shell, console
│   ├── drivers/          #   ATA, PS/2 keyboard, mouse
│   ├── fs/               #   FAT32
│   ├── lib/gui/          #   VESA framebuffer
│   ├── lib/owt/          #   Ovsb Widget Toolkit
│   └── lib/wm/           #   Window Manager
│
├── ../disp/              # Window Manager (userland ring 3, repo separado)
│
├── src/userland/         # Userland
│   ├── Makefile          #   .c → .macho → disk.img
│   ├── include/          #   libc headers
│   ├── libc/             #   stdio, stdlib, string, crt0, tui
│   ├── progs/            #   graphy.c e outros
│   └── tools/            #   macho_pack.py
│
├── docs/                 # Documentação
└── Discord_docc/         # Discord docs
```

## Build & Run

```bash
# Tudo de uma vez
make all                  # kernel .elf → ISO
make disk.img             # disco FAT32 (cria se não existir)
make userland             # compila + instala userland + disp-wm no disco
make run                  # QEMU (512M RAM, FAT32 disk)

# Ou separado
make kernel               # só kernel .elf
make iso                  # só ISO

# QEMU manual
qemu-system-x86_64 \
    -cdrom TipOS.iso \
    -drive file=disk.img,format=raw,index=0 \
    -boot order=d \
    -m 512M
```

## Shell (OvsbMkM)

| Comando      | Descrição                     |
|--------------|-------------------------------|
| `help`       | Lista comandos                |
| `clear`      | Limpa a tela                  |
| `echo <txt>` | Imprime texto                 |
| `info`       | Info do sistema (VESA, heap)  |
| `hexdump`    | Exibe 64 bytes de um endereço |
| `run`        | Executa programa ring 3 demo  |
| `exec <file>`| Carrega e executa Mach-O do FAT32 |
| `ls`         | Lista diretório FAT32         |
| `cd <dir>`   | Muda diretório FAT32          |
| `owt`        | Demo do OWT (widget toolkit)  |
| `reboot`     | Reinicia o sistema            |

Para rodar o disp-wm: `exec DISP` (tecle ESC para sair).

## Syscalls (int 0x80)

| Nº | Nome      | rax | rdi        | rsi        | rdx         | rcx      | Descrição            |
|----|-----------|-----|------------|------------|-------------|----------|----------------------|
| 1  | exit      | 1   | code       | -          | -           | -        | Retorna ao shell     |
| 3  | read      | 3   | fd         | buffer     | count       | -        | Teclado (fd=0)      |
| 4  | write     | 4   | fd         | buf        | count       | -        | Console (fd=1,2)    |
| 5  | open      | 5   | path       | flags      | mode        | -        | Arquivo             |
| 6  | close     | 6   | fd         | -          | -           | -        | Fecha fd            |
| 10 | unlink    | 10  | path       | -          | -           | -        | Remove arquivo      |
| 20 | getpid    | 20  | -          | -          | -           | -        | PID                 |
| 24 | getuid    | 24  | -          | -          | -           | -        | UID (sempre 0)      |
| 33 | access    | 33  | path       | mode       | -           | -        | Verifica acesso     |
| 47 | getgid    | 47  | -          | -          | -           | -        | GID (sempre 0)      |
| 54 | ioctl     | 54  | fd         | request    | -           | -        | Stub (ret 0)        |
| 73 | munmap    | 73  | addr       | length     | -           | -        | Libera mmap         |
| 74 | mprotect  | 74  | addr       | length     | prot        | -        | Stub (ret 0)        |
| 116| gettimeofday|116| tv         | -          | -           | -        | Timestamp           |
| 134| sigaction | 134 | signum     | act        | oldact      | -        | Stub                |
| 136| mkdir     | 136 | path       | mode       | -           | -        | Cria diretório      |
| 137| rmdir     | 137 | path       | -          | -           | -        | Remove diretório    |
| 173| sigreturn | 173 | -          | -          | -           | -        | Stub                |
| 188| stat      | 188 | path       | stat buf   | -           | -        | Info arquivo        |
| 189| fstat     | 189 | fd         | stat buf   | -           | -        | Info por fd         |
| 197| mmap      | 197 | addr       | length     | prot        | flags    | Aloca páginas       |
| 198| kbhit     | 198 | -          | -          | -           | -        | Tecla disponível?   |
| 199| lseek     | 199 | fd         | offset     | whence      | -        | Posiciona em fd     |
| 202| disp_get_fb| 202 | addr out  | width out  | height out  | pitch out| Endereço framebuffer|
| 203| disp_flush | 203 | backbuffer | -          | -           | -        | Flush framebuffer   |

Registradores: rax=nº, rdi=a1, rsi=a2, rdx=a3, rcx=a4. Retorno em rax.

## Userland

Programas em `src/userland/progs/` usam a libc TipOS:

- **stdio**: printf, fopen/fclose/fread/fwrite, getchar/putchar, kbhit, sprintf
- **stdlib**: malloc (freelist), calloc, realloc, free, atoi, itoa, exit
- **string**: memset, memcpy, memmove, strlen, strcmp, strncpy, strcat, strtok, strdup
- **ctype**: isdigit, isspace, isalpha, isprint, etc.
- **tui**: TUI library com double buffer, refresh parcial, janelas, widgets

### graphy — TUI Text Editor

Editor de texto no terminal com syntax highlight para C.

| Atalho   | Função                    |
|----------|---------------------------|
| ^O       | Salvar                    |
| ^X       | Sair                      |
| ^F       | Buscar                    |
| ^R       | Substituir                |
| ^Z       | Undo                      |
| ^Y       | Paste                     |
| ^K       | Kill line                 |
| ^T       | Alterna buffer            |
| ^J       | Ir para linha             |
| F2       | Line numbers toggle       |

## disp-wm

Compositor gráfico que roda como programa userland (ring 3).

```bash
# Build e instalação
make -C ../disp clean
make -C ../disp
make -C ../disp install TIPOS_SDK=$(pwd)

# No shell do TipOS:
#   exec DISP
```

Navegação: `WASD` move cursor, `Espaço` clica, `Tab` ciclo de foco,
`Ctrl+N` nova janela, `Ctrl+Q` fecha, `ESC` sai.

## Licença

- **Userland TipOS** (`src/userland/`): MIT
- **Kernel OvsbMk** (`OvsbMk/`): licença do autor original
