# OvsbOS

OvsbOS é um projeto pessoal de sistema operacional e ambiente de trabalho,
com foco em compatibilidade entre Linux, WSL e Windows, desempenho bruto,
estabilidade e simplicidade de desenvolvimento.

## Ecossistema

- **OvsbOS**: sistema operacional e ambiente de trabalho.
- **Ovsb K**: kernel x86_64, boot, memória, interrupções, drivers e processos.
- **Ovsb OWT**: front-end e framework leve para aplicações.
- **Ovsb WM**: biblioteca de janelas integrada ao OWT.
- **Ovsb SDK**: ferramentas para compilar, testar e instalar aplicações.

## Estrutura atual

```text
OvsbOs/
├── OvsbMk/        kernel, drivers, filesystem e boot
├── src/userland/  libc, TUI e aplicações nativas
├── docs/          arquitetura, syscalls, ELF e desenvolvimento
├── iso/           imagem de boot do GRUB
├── build/         artefatos gerados
└── tests/         validações e binários de teste
```

O código atual mantém compatibilidade com a organização legada do kernel:
`OvsbMk/` é o núcleo funcional e `src/userland/` contém programas de Ring 3.

## Arquitetura

- x86_64 em long mode, GRUB2 e Multiboot2.
- Kernel em Ring 0 com GDT, IDT, PIC, TSS e escalonador.
- Aplicações em Ring 3 com `CS=0x1B` e `SS=0x23`.
- Framebuffer VESA/VBE 32 bpp, backbuffer e compositor Ovsb WM.
- Mouse e teclado PS/2, ATA PIO, FAT32 e memória virtual por processo.
- Binários nativos Mach-O e compatibilidade ELF64 com tradução de syscalls.

Consulte [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) para o desenho dos
componentes e [docs/ELF.md](docs/ELF.md) para o carregamento de aplicações.

## Como começar

```bash
make kernel
make iso
make run
```

Outros fluxos:

```bash
make userland   # compila e instala aplicações no disk.img
make run-test   # boot headless; log em /tmp/ovsbos-boot.log
make run-curses # QEMU em modo texto
```

Dependências principais: `gcc`, `make`, `nasm`, `zig`, `grub-mkrescue`,
`mtools`, `qemu-system-x86_64` e `xorriso`.

## Shell

O shell do Ovsb K oferece `help`, `clear`, `echo`, `info`, `hexdump`, `exec`,
`ls`, `cd`, `owt` e `reboot`. O compositor pode ser iniciado com:

```text
ovsb> exec DISP
```

## Syscalls

Todas as chamadas nativas usam `int 0x80`, com número em `RAX`, argumentos em
`RDI`, `RSI`, `RDX`, `RCX` e retorno em `RAX`. A tabela completa está em
[docs/SYSCALLS.md](docs/SYSCALLS.md).

## Documentação

- [Arquitetura](docs/ARCHITECTURE.md)
- [Syscalls](docs/SYSCALLS.md)
- [ELF e Mach-O](docs/ELF.md)
- [Build e execução](docs/BUILD.md)

OvsbOS é um projeto pessoal em desenvolvimento ativo, com objetivo técnico,
educacional e de construção de um ambiente de trabalho funcional.
