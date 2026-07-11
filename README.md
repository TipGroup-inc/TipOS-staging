# TipOS

Sistema operacional TipOS — userland sobre o kernel OvsbMkM.

## Estrutura do projeto

```
TipOS/
├── Makefile              # Build principal
├── README.md
├── disk.img              # FAT32 (gerado, gitignored)
├── TipOS.iso             # ISO bootável (gerado, gitignored)
│
├── OvsbMkM/              # Kernel (Bugsappetit.inc) — intocável
│   ├── src/kernel/       #   kmain, IDT, syscalls, Mach-O loader, dyld
│   ├── src/drivers/      #   ATA, PS/2 keyboard
│   ├── src/fs/           #   FAT32
│   ├── src/commands/     #   Shell + exec
│   └── macos_bins/       #   Binários Mach-O embedados
│
├── src/
│   ├── dock/             # Dock HAL (ABI, manifesto, sandbox)
│   │   ├── core/         #   loader, lifecycle, manifest, registry
│   │   ├── abi/          #   resolver
│   │   ├── api/          #   (futuro)
│   │   ├── vfs/          #   (futuro)
│   │   ├── utils/        #   log
│   │   └── include/      #   dock.h
│   │
│   └── userland/         # Userland TipOS
│       ├── Makefile      #   Compila .c → .macho → instala no disco
│       ├── progs/        #   hello.c
│       ├── libc/         #   Libc TipOS (futuro)
│       ├── disp/         #   Display server / WM (futuro)
│       ├── bin/          #   (futuro)
│       └── tools/        #   macho_pack.py
│
└── docs/                 # Documentação de arquitetura
    ├── tipos-vision.md
    ├── tipos-doca.md
    └── ...
```

## Build & Run

```bash
make              # kernel + userland + ISO + disco
make run          # tudo + QEMU com serial (-serial stdio)
make run-curses   # tudo + QEMU no terminal (-display curses)
make clean        # limpa artefatos
```

Os targets `run` e `run-curses` sobem QEMU com o ISO e o disco FAT32
automaticamente. QEMU 10.2.2 instalado via pacote oficial do Slackware.

Se quiser rodar manualmente (útil quando já tem o ISO):

```bash
qemu-system-x86_64 \
    -boot order=d \
    -cdrom TipOS.iso \
    -m 256M \
    -drive file=disk.img,format=raw,if=ide
```

Com aceleração KVM (mais rápido, precisa estar no grupo `qemu`):

```bash
qemu-system-x86_64 -enable-kvm -boot order=d -cdrom TipOS.iso \
    -m 256M -drive file=disk.img,format=raw,if=ide
```

## Userland

Programas em `src/userland/progs/` são compilados para Mach-O,
copiados para a FAT32 (`disk.img`) e executados no shell do kernel
com `exec NOME`.

Comandos do kernel: `help, clear, echo, about, shutdown, ls, touch,
rm, cat, edit, mkdir, cd, pwd, exec`

## Syscalls (int 0x80, estilo XNU)

| Número | Nome           | Descrição                     |
|--------|----------------|-------------------------------|
| 1      | exit           | Não implementado (noop)       |
| 3      | read           | Lê teclado (fd=0)             |
| 4      | write          | Escreve VGA (fd=1,2)          |
| 5      | open           | Só "/dev/tty"                 |
| 6      | close          | Noop                          |
| 197    | mmap           | Aloca páginas do heap         |
| 73     | munmap         | Libera páginas                |

Chamada: `rax=número, rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4; int 0x80`

## Licença

- **Userland TipOS e Dock HAL**: MIT
- **Kernel OvsbMkM**: licença do autor original (Bugsappetit.inc)
