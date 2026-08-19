# Documentacao OvsbOS

Documentacao tecnica atual do OvsbOS. O codigo de referencia fica em
`OvsbMk/` e `src/userland/`.

## Comece aqui

| Documento | Conteudo |
|---|---|
| [README](../README.md) | Identidade, componentes e comandos principais |
| [ARCHITECTURE](ARCHITECTURE.md) | Boot, camadas, video, entrada e Rings |
| [SYSCALLS](SYSCALLS.md) | Tabela nativa e compatibilidade Linux |
| [ELF](ELF.md) | Loader ELF64, Mach-O, stacks e ABI |
| [BUILD](BUILD.md) | Dependencias, build, QEMU e testes |

## Fontes de verdade

- Syscalls: `OvsbMk/kernel/syscall.h` e `OvsbMk/kernel/syscall.c`.
- Loader ELF: `OvsbMk/kernel/elf64.zig`.
- Traducao Linux: `OvsbMk/kernel/syscall_linux.zig`.
- Processos e Rings: `OvsbMk/kernel/process.c` e `process.h`.
- Video: `OvsbMk/lib/gui/vesa.c` e `OvsbMk/lib/wm/wm.c`.

Documentos antigos de planejamento e canais internos nao fazem parte da
publicacao tecnica atual.
