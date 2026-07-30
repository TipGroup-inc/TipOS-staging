/* moe moe kyun <3 */
/* moe moe kyun <3 */
🪟 **WINDOW SERVER — TipOS**

**Status: protótipo inicial existe.** `src/userland/disp/compositor.c` — compositor gráfico básico, ainda não integrado ao fluxo principal do shell.

**Comando `disp` já existe no shell** (`cmd_disp()`, `shell_cmds.c`) — entra em modo gráfico, mas hoje é bem limitado.

---

**Plano completo (`tipos-equipes-e-estrutura.md`, Time D):**
- Display server: processo dono do framebuffer
- Compositor: janelas, Z-order, título, cursor por software
- Eventos de teclado/mouse roteados via IPC (hoje o teclado vai direto pro shell — precisa desacoplar)
- Workspaces (F1–F6)
- Barra de status (relógio via RTC, sessões, load)
- Tiling WM — bem no futuro

**Milestone do Time D:** "Interface gráfica com 2 apps rodando."

---

**Dependências bloqueantes:**
- IPC (#ipc) — pra rotear eventos de input sem o compositor precisar ler o hardware direto
- Driver de vídeo além do modo texto (#video) — framebuffer VESA ou pelo menos o modo gráfico 320x200 já existente (`vga_gfx.c`) integrado

Discussão em aberto: compositor single-threaded no MVP (mais simples, já que ainda não há scheduler/threads) ou já pensar em multi-thread quando o scheduler existir?
