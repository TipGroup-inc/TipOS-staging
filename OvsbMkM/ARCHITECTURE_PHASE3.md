# Arquitetura Técnica - Fase 3 do OvsbMkM

## Status
- Implementado parcialmente: camada de syscalls XNU básica e ambiente de execução para binários Mach-O.
- Foco: `src/kernel/syscall.c`, `src/kernel/idt.c`, `src/kernel/syscall_entry.asm`, `src/kernel/memory.c`.

## Objetivo
Suportar um subconjunto de chamadas de sistema xnu necessárias para que um binário macOS carregado (como `bash`) execute usando a interface do kernel.

## Artefatos principais
- `src/kernel/syscall.c` — dispatch de syscalls e stubs XNU.
- `src/kernel/idt.c` — inicialização de IDT e syscall trap.
- `src/kernel/syscall_entry.asm` — entrada de syscall em 64-bit.
- `src/kernel/memory.c` — implementação de gerenciador de memória para `mmap`/`munmap`.
- `src/kernel/kernel.c` — habilita `idt_set_syscall` antes de iniciar o binário.

## Syscalls suportadas
- `SYS_write` — saída para VGA/TTY (`fd == 1 || fd == 2`).
- `SYS_read` — leitura de teclado para `fd == 0`.
- `SYS_open` / `SYS_close` — stubs que aceitam `/dev/tty`.
- `SYS_access` — valida `/dev/tty`, `/dev/stdin`, `/dev/stdout`, `/dev/stderr`.
- `SYS_stat`, `SYS_fstat`, `SYS_lstat` — retornam sucesso para descritores de terminal.
- `SYS_mmap` / `SYS_munmap` / `SYS_mprotect` — mapeamento de memória básico.
- `SYS_getpid`, `SYS_getuid`, `SYS_geteuid`, `SYS_getgid`, `SYS_getegid` — retornos estáticos.
- `SYS_ioctl` — stub neutro.
- `SYS_sigaction`, `SYS_sigreturn` — aceitam chamadas mas não fazem tratamento real.
- `SYS_gettimeofday` — preenche `timeval` com zeros.

## Fluxo da syscall
1. Processo usuário executa `syscall` no modo usuário.
2. `syscall_entry.asm` salta para o handler em C.
3. `syscall_handler()` seleciona a rotina por número.
4. Resultado é retornado para o chamador do binário.

## Pontos importantes
- `SYS_write` escreve diretamente na tela via `vga_putchar`.
- `SYS_read` lê um caractere por vez do teclado e termina quando recebe `\n`.
- `SYS_mmap` usa `mmap_user` para alocar um bloco de memória no espaço do kernel para uso do binário.
- `SYS_open` não usa filesystem real; apenas aceita terminal e caminhos virtuais.

## Limitações atuais
- Sem gerenciamento de processos reais ou tabelas de descritores de arquivos completas.
- Sem sinalização real de processos (`sigaction` é stub).
- Sem ambiente de usuário completo (`argv`, `environ`, `stdin`/`stdout` reais).
- Sem retorno de tempo real em `gettimeofday`.
- Sem proteção de memória separada para o processo do usuário.

## Resultado desejado
- Binário macOS executando chamadas `write` e `read` com feedback no kernel.
- Binário carregado dependendo minimamente de stubs de filesystem e I/O.
- Capacidade de iniciar a execução do Mach-O embarcado e retornar ao kernel se o binário terminar.

## Próximos passos dessa fase
- Implementar tabela de processos/threads.
- Adicionar suporte a arquivos reais e sistema de dados em memória.
- Suportar memória de usuário isolada por page tables.
- Adicionar handling de sinais e timers.
- Suportar `fork`/`execve` e outros syscalls BSD essenciais.
