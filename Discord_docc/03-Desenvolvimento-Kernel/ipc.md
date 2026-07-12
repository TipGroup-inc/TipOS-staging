📡 **IPC — TipOS**

**Status atual: não existe ainda.** Kernel monolítico single-address-space não precisa de IPC pra se comunicar consigo mesmo — mas é bloqueante pra tudo que envolve mover drivers/servidores pra user-space (Doca/HAL, VFS, WindowServer, netstack...).

---

**Decisão já tomada (`tipos-dev-stack.md`, seção 6):**
> IPC estilo **Mach** — portas, mensagens assíncronas, memória compartilhada.
> Alternativa cogitada e descartada por ora: seL4-style sync IPC (mais rápido, API diferente).

**Arquivo planejado:** `kernel/ipc/mach_ipc.c` (novo, 0% implementado)

**Escopo do primeiro protótipo (Marco 2 / Fase 1 do roadmap em equipe):**
- Portas e mensagens básicas
- Canal de comunicação kernel↔processo via porta
- Primeiro caso de uso real: enviar IRQ do teclado pra um processo servidor (em vez do handler tratar tudo inline como hoje)

---

**Mecanismos que hoje fazem o papel de "IPC" de forma improvisada, dentro do shell:**
- Pipes (`|`) — hoje só mostra `[pipe not fully implemented yet]`. Implementação real precisa de um "teclado virtual" que leia do buffer de saída do primeiro comando (`redir_buf`) como entrada do segundo.
- Redirecionamento (`>`, `>>`) — já funciona, interceptando `vga_putchar()` num buffer.

**Fora do shell, planejados:** clipboard global (^C/^V entre programas), sinais (`SIGKILL`, `SIGTERM`, `SIGINT`, `SIGSEGV`), `signal()`/`sigaction()` — hoje `sigaction`/`sigreturn` são apenas stubs (syscalls 134 e 173).

Quem quiser puxar essa frente, o `mach_ipc.c` é o ponto de partida — nada implementado ainda, terreno livre pra desenhar a API.
