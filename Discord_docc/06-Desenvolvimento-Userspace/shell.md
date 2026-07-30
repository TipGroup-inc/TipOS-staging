/* moe moe kyun <3 */
/* moe moe kyun <3 */
🐚 **SHELL — TipOS**

**Prompt:** `MkM> ` — roda inline no kernel (`shell_loop()`, `kernel.c`), ainda não é um processo separado.

**Comandos builtin (22):**
```
help clear echo about shutdown ls touch rm cat edit mkdir cd pwd
mv cp rmdir stat exec disp date uptime sleep
```

**Já funcional:**
- **History:** 128 entradas circular (`HIST_MAX`), navegação com ↑↓ (`browse_idx`)
- **Line editing:** ←/→, Home/^A, End/^E, Del, Backspace, ^K (kill to end), ^U (kill to start), ^W (kill word), ^L (clear screen mantendo comando)
- **PATH search:** `/BIN/`, `/USR/BIN/`, `/LOCAL/BIN/` — comando não-builtin busca automaticamente (`GRAPHY` funciona sem `exec /BIN/GRAPHY`)
- **Redirecionamento:** `>` (write) e `>>` (append) funcionam de verdade, interceptando `vga_putchar()` num buffer e escrevendo no FAT32 depois
- **Autocomplete (TAB):** completa builtins + PATH na 1ª palavra, arquivos/dirs do CWD nas seguintes. Match único completa direto; múltiplos matches preenchem o prefixo comum; TAB duplo lista todos os matches.
- **^C** interrompe o comando atual

**Ainda não funcional:**
- **Pipe (`|`)** — hoje só mostra `[pipe not fully implemented yet]`. Pra implementar de verdade: capturar saída do 1º comando no `redir_buf` e alimentar como "teclado virtual" pro 2º comando.
- Prompt customizável (PS1-like), aliases, background jobs (`&`, `jobs`, `fg`, `bg`), variáveis de ambiente (`$PATH`, `$HOME`, `$EDITOR`), scripting (`.sh`)

---

**Como adicionar um comando novo** (`KERNEL.md`, seção 18):
```c
// 1. shell_cmds.h
void cmd_meucomando(void);

// 2. shell_cmds.c
void cmd_meucomando(void) { vga_puts("..."); }

// 3. kernel.c (execute_command)
else if (strcmp(work, "meucomando") == 0) cmd_meucomando();
// + adicionar em cmd_help()
```

Testem os comandos e reportem bugs de line editing/history aqui!
