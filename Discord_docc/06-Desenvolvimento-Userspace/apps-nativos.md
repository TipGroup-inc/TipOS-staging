📦 **APPS NATIVOS — TipOS**

**Post inicial: `graphy` rodando**
[anexar screenshot com syntax highlight ativo]

`graphy` é o editor TUI do TipOS (~620 linhas, `src/userland/progs/graphy.c`), roda como Mach-O userland:

**Atalhos:** ^O salvar · ^X sair · ^G toggle help · ^F find · ^S/^R replace global · ^J go to line · ^Z undo (512 ops) · ^W cut word · ^Y paste · ^K kill line · ^T toggle buffer · ^C command mode · F2 toggle números de linha

**Syntax highlighting** via state machine (normal / string / line comment / block comment / preprocessor) — strings verdes, comentários vermelhos, preprocessor ciano, keywords amarelas, números magenta, números de linha cinza — tudo via parser ANSI do VGA.

**Limitações conhecidas:** buffer de 64KB (`KBUF`), máximo 4096 linhas (`MAXLNS`) — arquivos maiores truncam ou dão overflow.

---

**Como escrever um app novo** (`tipos-tutorial.md`):

```c
#include <stdio.h>
int main(int argc, char **argv) {
    printf("Hello, TipOS!\n");
    return 0;
}
```
Adiciona no `Makefile` (`PROGS = graphy hello`), `make -C src/userland install` empacota como Mach-O e copia pro `/BIN/` do `disk.img`. Roda com `HELLO` direto no shell (PATH search automático).

**Libc disponível:** `stdio.h` (printf, fopen/fread/fwrite, getchar/kbhit), `stdlib.h` (malloc bump allocator, atoi, exit), `string.h`, `ctype.h`, `sys/stat.h`.

---

**Roadmap de apps (ainda não implementados):**
- `hello` — programa exemplo (já no tutorial)
- **TCC** (Tiny C Compiler) portado + comandos `cc`, `make`, `as`
- **Git** portado (subset: init, add, commit, log, diff, branch, checkout, push, pull)
- Debugger TUI (tipo gdb/lldb)
- `grep`, `find`, `locate`, fzf-like
- `proj` — workspace manager nativo (LSP client, compile, quickfix)
- `tipkg` — package manager
- `top`/`htop`, `ps`, `dmesg` (uptime e date já existem como builtins do shell)

Testou algum app? Posta print ou vídeo aqui.
