# TipOS v0.7.4.0

Sistema operacional x86-64 com kernel OvsbMk, libc própria, compositor gráfico,
editor TUI e suporte a execução de binários **musl-linked static PIE ELF**
(Linux x86-64 compat).

```
/
├── OvsbMk/          ─── KERNEL (ring 0)     ─── C + ASM + Zig
└── src/userland/    ─── Userland (ring 3)   ─── C freestanding
```

## Subsystems

### OvsbMk — Kernel
- Boot por GRUB2 (Multiboot2), long mode 64-bit, PML4 identity mapping
- 30 syscalls via int 0x80 (convenção XNU), suporte a `syscall` instruction (MSR LSTAR)
- Ring 3 funcional (TSS, iretq), scheduler round-robin, PCB estático (64 slots)
- Memória: bump + buddy (4KB frames) + SLAB, mmap_user para userland
- FAT32 completo (read/write/create/delete), ext2 em progresso, initramfs
- Drivers: ATA PIO, PS/2 keyboard+mouse, PCI, Virtio GPU, USB (stub)
- GUI: VESA framebuffer 1024x768x32, OWT widgets (button, label, textbox, etc.), WM multi-janela
- Shell MkM> com 20+ comandos, history, autocomplete, PATH, aliases, background jobs

### src/userland — Userland
- libc freestanding: stdio (printf, fopen/fread/fwrite), stdlib (malloc), string, ctype, TUI
- Programas: graphy (editor TUI com syntax highlight C), disp-wm (compositor ring 3)
- Build: C → ELF → binary → Mach-O 64-bit, instalado no disco FAT32

## Build & Run

```bash
# Tudo
make all                  # kernel.elf → TipOS.iso
make disk.img             # FAT32 64MB (cria se não existir)
make userland             # compila + instala userland no disco
make run                  # QEMU (512MB, VGA std, serial stdio)

# Ou passo a passo
make kernel               # só kernel.elf
make iso                  # só ISO
make run-curses           # QEMU modo texto (sem X11)
make run-test             # headless, log em /tmp/tipos-boot.log

# QEMU manual
qemu-system-x86_64 -cdrom TipOS.iso -drive file=disk.img,format=raw,index=0 -m 512M
```

## Shell (MkM>)

| Comando      | Descrição                     |
|--------------|-------------------------------|
| `help`       | Lista comandos                |
| `clear`      | Limpa a tela                  |
| `echo <txt>` | Imprime texto                 |
| `info`       | Info do sistema (VESA, heap)  |
| `hexdump`    | Exibe 64 bytes de um endereço |
| `exec <file>`| Carrega e executa Mach-O ou ELF64 do FAT32 |
| `exec HELLO`| Demo ELF: "Hello from musl ELF!" (Linux compat) |
| `ls`         | Lista diretório FAT32         |
| `cd <dir>`   | Muda diretório FAT32          |
| `owt`        | Demo do OWT (widget toolkit)  |
| `reboot`     | Reinicia o sistema            |

Para rodar o disp-wm: `exec DISP` (ESC para sair).

## Syscalls (int 0x80)

rax=nº, rdi=a1, rsi=a2, rdx=a3, rcx=a4. Retorno em rax.

| Nº | Nome         | rdi         | rsi          | rdx       | rcx       |
|----|--------------|-------------|--------------|-----------|-----------|
| 1  | exit         | code        | -            | -         | -         |
| 3  | read         | fd          | buffer       | count     | -         |
| 4  | write        | fd          | buf          | count     | -         |
| 5  | open         | path        | flags        | mode      | -         |
| 6  | close        | fd          | -            | -         | -         |
| 10 | unlink       | path        | -            | -         | -         |
| 20 | getpid       | -           | -            | -         | -         |
| 24 | getuid       | -           | -            | -         | -         |
| 33 | access       | path        | mode         | -         | -         |
| 47 | getgid       | -           | -            | -         | -         |
| 54 | ioctl        | fd          | request      | -         | -         |
| 73 | munmap       | addr        | length       | -         | -         |
| 74 | mprotect     | addr        | length       | prot      | -         |
| 116| gettimeofday | tv          | -            | -         | -         |
| 134| sigaction    | signum      | act          | oldact    | -         |
| 136| mkdir        | path        | mode         | -         | -         |
| 137| rmdir        | path        | -            | -         | -         |
| 173| sigreturn    | -           | -            | -         | -         |
| 188| stat         | path        | stat buf     | -         | -         |
| 189| fstat        | fd          | stat buf     | -         | -         |
| 197| mmap         | addr        | length       | prot      | flags     |
| 198| kbhit        | -           | -            | -         | -         |
| 199| lseek        | fd          | offset       | whence    | -         |
| 202| disp_get_fb  | addr out    | width out    | height out| pitch out |
| 203| disp_flush   | backbuffer  | -            | -         | -         |

## Linux x86-64 ELF Compatibility

TipOS v0.7.4.0 can run **musl-linked static PIE ELF64 binaries** natively:

- **ELF loader** (`elf64.zig`): loads ELF64 into a child PML4, supports PT_LOAD segments with 2MB hugepages
- **Syscall translation** (`syscall_linux.zig`): maps Linux syscall numbers (e.g., read=0, write=1, exit_group=231) to TipOS native numbers
- **Auxiliary vector**: `setup_linux_user_stack()` pushes AT_RANDOM, AT_PAGESZ, AT_SECURE, AT_PHNUM, AT_PHENT, AT_PHDR
- **TLS**: FS.base MSR save/restore per process (`switch.asm`), `arch_prctl` stub
- **Demo binary**: `HELLO` prints "Hello from musl ELF!" and exits cleanly

## Licença
- Kernel OvsbMk (`OvsbMk/`): licença do autor original
- Userland TipOS (`src/userland/`): MIT
