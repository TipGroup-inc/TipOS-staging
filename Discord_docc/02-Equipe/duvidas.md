/* moe moe kyun <3 */
❓ **DÚVIDAS — TipOS**

Formato sugerido pra perguntas técnicas:

```
**Contexto:** o que você está tentando fazer
**Onde no código:** arquivo/função (ex: syscall.c:117, fat32.c)
**O que já tentou:** passos ou trecho de código
**Erro/comportamento:** log, crash, ou comportamento inesperado
```

**Antes de perguntar, dá uma olhada em `KERNEL.md` — seção "Problemas Comuns"**, já cobre:
- `Kernel panic: no working init found` → FAT32 não inicializou ou disco não montado
- `Comando nao encontrado` pra binários em `/BIN/` → `mcopy` falhou ou Mach-O inválido
- Tela cheia de caracteres estranhos → parser ANSI não está sendo chamado em `vga_putchar()`
- Teclas não respondem → IRQ1 não habilitado / `sti` não executado
- Build falha com `inw %edx` → GCC 15+, precisa do `sed` no Makefile (já tratado)
- `graphy` crasha em arquivo grande → limite de 64KB (`KBUF`) / 4096 linhas (`MAXLNS`)

Trade-offs de design (ex: "qual scheduler escolher?", "Mach IPC ou algo mais simples?") também têm espaço aqui — só citar contexto suficiente pra virar discussão.
