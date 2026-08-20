# Build e execucao

## Dependencias

Linux/WSL:

```bash
sudo apt install gcc make nasm qemu-system-x86 mtools grub2-common xorriso
```

Tambem e necessario o Zig para os drivers e loaders Zig.

## Comandos

```bash
make kernel      # compila Ovsb K
make iso         # gera a ISO GRUB
make disk.img    # cria a imagem FAT32
make userland    # compila e instala apps
make run         # inicia QEMU com video e serial
make run-curses  # inicia QEMU em modo texto
make run-test    # boot headless, log em /tmp/ovsbos-boot.log
```

Fluxo recomendado:

```bash
make kernel
make iso
make userland
make run
```

## Testes

Para verificar sem abrir uma janela:

```bash
timeout 5s make run-test
cat /tmp/ovsbos-boot.log
```

O log esperado inclui `[VIDEO]`, `[RING]`, `Hello from musl ELF!` e o inicio
do compositor. Um `qemu_exit=124` indica apenas que o `timeout` encerrou o
QEMU.

## Artefatos

- `OvsbMk/build/kernel.elf`: kernel linkado.
- `OvsbOS.iso`: imagem GRUB gerada pelo Makefile atual.
- `disk.img`: FAT32 de 64 MiB com programas em `/BIN`.
- `build/userland/`: saidas intermediarias das aplicacoes.

Os nomes dos artefatos legados serao renomeados gradualmente para OvsbOS em
uma etapa separada, sem alterar o formato de boot.
