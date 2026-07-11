# OvsbMkM

Status: Protótipo de kernel 64-bit em desenvolvimento. As fases 1 a 3 estão documentadas e a base de execução de binários Mach-O já foi integrada.

## Visão geral atual
- Fase 1: Terminal VGA interativo, teclado PS/2, boot GRUB + Multiboot2.
- Fase 2: Binários macOS embutidos (`bash`, `ls`) e loader Mach-O básico.
- Fase 3: Camada de syscalls XNU parcial para suporte a execuções de usuários.

## Build & Run

```bash
make clean && make
make run    # boots in QEMU
```

## Principais arquivos do estado atual
- `src/kernel/kernel.c` — bootstrap, inicialização e chamada de binário embutido
- `src/kernel/mach_o.c` + `src/kernel/mach_o.h` — parser/loader Mach-O 64-bit
- `src/kernel/syscall.c` — dispatcher XNU para `read`, `write`, `mmap`, `open`, `stat`, etc.
- `src/kernel/bash_bin.c` — `/bin/bash` embutido como array C
- `src/kernel/ls_bin.c` — `/bin/ls` embutido como array C
- `src/kernel/memory.c` — suporte a mmap/munmap para binários de usuário
- `src/kernel/pic.c` — PIC init corrigido para restaurar máscaras
- `Makefile` — inclui binários embutidos no build

## Documentação de fases
- `ARCHITECTURE_PHASE1.md` — Fase 1: boot, terminal e memória 64-bit
- `ARCHITECTURE_PHASE2.md` — Fase 2: Mach-O loader e binários macOS embutidos
- `ARCHITECTURE_PHASE3.md` — Fase 3: syscalls XNU e ambiente de execução

## Arquitetura rápida
1. GRUB carrega `build/kernel.elf`
2. `boot64.asm` configura 64-bit, paginação e GDT
3. `kmain()` inicializa VGA, teclado, IDT, PIC e memória
4. O kernel chama `mach_o_load(bash_bin, bash_bin_len)`
5. Segmentos Mach-O são copiados para memória em `slide = 0x2000000`
6. A entrypoint do Mach-O é encontrada e executada
7. Syscalls XNU são despachadas por `syscall_handler()`

## Limitações conhecidas
- Loader Mach-O protótipo: não há relocations nem suporte a bibliotecas dinâmicas
- Syscall layer parcial: muitas chamadas XNU ainda são stubs
- Nada de memória de usuário isolada ou processos reais
- O binário macOS depende de stubs de I/O e `tty`
- Linker ainda emite warnings de pilha executável e RWX

## Como ajudar / próximos passos
- Completar relocations Mach-O e `dyld` mínimo
- Implementar `execve`, `fork`, `wait`, `readlink`, `open` real e VFS
- Criar tabela de processos e espaço de endereço separado
- Adicionar tratamento real de sinais e `gettimeofday`

## Projeto em uma imagem
```
OvsbMkM/
├── ARCHITECTURE_PHASE1.md
├── ARCHITECTURE_PHASE2.md
├── ARCHITECTURE_PHASE3.md
├── EXPECTED_OUTPUT.md
├── QUICKSTART.md
├── TROUBLESHOOTING.md
├── Makefile
├── README.md
├── src/
│   ├── kernel/
│   │   ├── bash_bin.c
│   │   ├── kernel.c
│   │   ├── mach_o.c
│   │   ├── mach_o.h
│   │   ├── syscall.c
│   │   ├── memory.c
│   │   ├── pic.c
│   │   └── ...
│   └── drivers/
│       └── keyboard.c
└── iso/
```

> Atualizado para documentar as fases 1–3 e refletir o estado atual do código.
