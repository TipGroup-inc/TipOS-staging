# Mudanças Realizadas no TipOS

## Resumo
Correções de compatibilidade para permitir compilação em sistemas com versões mais recentes de GCC/binutils.

## Arquivos Modificados

### 1. Makefile (raiz)
- Repositorios irmaos (disp/term) agora sao opcionais

### 2. OvsbMk/drivers/ata.c
- Corrigido inline assembly: %edx -> %dx (16-bit)

### 3. OvsbMk/drivers/usb.c
- Corrigido inline assembly: %edx -> %dx (16-bit)

### 4. src/userland/Makefile
- Comentarios C -> comentarios Makefile
- PROGS = graphy (adicionado)

### 5. src/userland/libc/link.ld
- Comentarios // -> /* */

### 6. src/userland/tools/macho_pack.py
- Comentarios C -> comentarios Python

### 7. OvsbMk/Makefile
- Caminho do Zig: hardcoded -> $(HOME)

## Resultado
- Kernel compila sem erros
- graphy compilado (24396 bytes)
- TipOS roda no QEMU

## Como testar
make clean
make all
make disk.img
make userland
make run

---

# Sessão 22/08/2026 — Xorg sobe como servidor + fundação de processos

Versão: v0.7.4.0 → v0.7.5.0
Branch: fs-terminal/67-ext2-clean-zig

## Resumo
O Xorg 21.1.13 (musl static-PIE) passou a inicializar como servidor
gráfico no TipOS: carrega keymap XKB pré-compilado, inicializa o
teclado, cria pipes de comunicação e lê /dev/input/mice. Para isso
foi preciso implementar readv/writev, consertar vm_munmap, adicionar
demand paging e aplicar 4 patches binários no executável do Xorg.

## Kernel

### syscall.c
- SYS_readv (Linux 19) novo: iovecs sobre vfs_read_at (arquivos) e
  ring buffer (pipes). Sem ele, todo fread do musl retornava EOF
  fantasma — o __stdio_read do musl sempre usa readv.
- SYS_execve (208) reescrito: VFS + ELF static-PIE, troca de CR3,
  kframe reescrito, fds >= 3 fechados (semântica POSIX).
- SYS_fork_real (214) novo, ligado ao Linux fork(57): clona espaço
  de usuário (walker de page tables, huge pages divididas em 4KB),
  copia fd table e monta frame iretq completo com RAX=0 no filho.
- fd tables por processo: pool estático em BSS (fd_tables[MAX_PROC]),
  macro `fds` preserva os usos existentes; bind lazy por syscall,
  duplicação no fork, close >= 3 no execve/exit.

### memory.c / vm_map.c / idt.c
- vm_munmap: libera frames do objeto (evitava vazamento por ciclo
  mmap/munmap) e invlpg em todas as páginas do range.
- Demand paging: PF handler do usuário aloca frame zerado para página
  não-presente coberta pelo vm_map, criando tabelas intermediárias;
  invlpg + retry da instrução.
- vm_map_covers() exportado para checagem de cobertura.

### process.c / switch.asm
- proc_fork(): cópia eager do espaço de usuário (walker de page
  tables, máscara de PA corrigida para bits 51:12), frame iretq
  completo do filho com RAX=0, vm_map duplicado.
- FS_BASE corrigido em switch.asm: 0xA8 -> 0xB8 (offset real de
  fs_base no PCB; o valor antigo apontava para o campo vm_map).

### pipe/pipe2/socketpair
- alloc_fd duplo marcava used apenas depois das duas chamadas,
  fazendo rfd == wfd == 3. Corrigido marcando entre os allocs.
- close_fd não destrói mais o objeto pipe ao fechar uma única ponta;
  EOF do read usa pipe_writers_alive(), que varre as fd tables dos
  processos vivos.

## Userland / disco
- /bin/sh mínimo (sh -c, estático musl)
- /bin/xkbcomp 1.4.7 rebuildado (o anterior estava zerado)
- def.xkm: keymap compilado instalado em share/X11/xkb/compiled/

## Binário do Xorg (4 patches documentados em docs/XORG-XKB-CAÇADA.md)
1. FBDevPreInit: jne -> jmp após LoadSubModule("shadow") falho
2. RunXkbComp: retorna "def" imediatamente (pula Popen/fork)
3. rodata "/bin/sh" substituída por "def" (string sacrificada)
4. Todos os call free do retorno NOPados (5 sites)

## Resultado
[XKMDIAG] missing=0x0 xkbRtrn=0x72200880
Pipes X11 criados, /dev/input/mice aberto, framebuffer renderizando.

## Próximos passos
- clone(56)/threads (última syscall antes do fim do boot do servidor)
- Estabilidade do heap sob churn (demand paging mascarando causas)
- Remoção dos prints de debug instrumentados
