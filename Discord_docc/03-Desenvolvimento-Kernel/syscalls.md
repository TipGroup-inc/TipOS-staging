/* moe moe kyun <3 */
🔧 **SYSCALLS — TipOS**

**Convenção: XNU-style**
```
Entrada:  RAX = número, RDI = a1, RSI = a2, RDX = a3, RCX = a4
Saída:    RAX = retorno
Gate:     int 0x80 (DPL=3, chamável de userland)
```

**30 syscalls implementadas hoje** (`syscall.c`):

| # | Nome | Descrição |
|---|------|-----------|
| 1 | exit | Termina processo |
| 3 | read | Lê fd 0 (teclado) ou arquivo |
| 4 | write | Escreve fd 1/2 (VGA) ou arquivo |
| 5 | open | Abre arquivo ou /dev/* |
| 6 | close | Fecha fd |
| 10 | unlink | Remove arquivo |
| 20 | getpid | Retorna 1 (stub) |
| 24/25 | getuid/geteuid | Retorna 0 |
| 33 | access | Stub |
| 47/48 | getgid/getegid | Retorna 0 |
| 54 | ioctl | Stub |
| 73 | munmap | Libera páginas |
| 74 | mprotect | Stub |
| 116 | gettimeofday | Relógio real via RTC |
| 134 | sigaction | Stub |
| 136/137 | mkdir2/rmdir2 | FAT32 |
| 173 | sigreturn | Stub |
| 188/189/199 | stat/fstat/lstat | Info de arquivo |
| 197 | mmap | Aloca páginas anônimas |
| 198 | kbhit | Tecla disponível? |
| 200 | lseek | Posiciona em arquivo |

**Fluxo:**
```
Userland: RAX=num, RDI/RSI/RDX/RCX=args → int 0x80
  → syscall_entry.asm: salva regs, rearranja pra C calling convention
  → syscall_handler(num, a1, a2, a3, a4) → switch(num) → handler
  → syscall_entry.asm: restaura regs → iretq → volta pra userland
```

---

**Como adicionar uma syscall nova** (`KERNEL.md`, seção 17):
```c
// 1. syscall.c
#define SYS_minha_syscall 999

// 2. dentro do switch em syscall_handler()
case SYS_minha_syscall:
    return resultado;

// 3. userland — wrapper
static inline int64_t minha_syscall(int a1, int a2) {
    int64_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(999), "D"(a1), "S"(a2) : "memory");
    return ret;
}
```

**Roadmap:** expandir de 30 pra 150+ syscalls (Fase 1 do roadmap em equipe), depois migrar o gate de `int 0x80` pra `syscall`/`sysret` (mais rápido, decisão em `tipos-dev-stack.md`). Também em avaliação: manter convenção XNU ou migrar pra ABI SysV.

Muitos stubs aqui (`ioctl`, `mprotect`, `sigaction`, `access`) — bom ponto de entrada pra quem quer contribuir sem mexer em nada crítico.
